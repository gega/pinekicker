#!/bin/bash
ELF=$1
OUT=$2
KEY=$3

if [ x"$OUT" == x"" ]; then
  OUT="out.bin"
fi

if [ x"$KEY" == x"" ]; then
  KEY="bootkey.pem"
fi

if [ ! -f "$ELF" ]; then
  echo "Usage: $0 fw.elf out.bin bootkey.pem"
  exit 1
fi

TMPSHA=$(mktemp)
TMPDER=$(mktemp)
TMPSIG=$(mktemp)
TMPOUT=$(mktemp)
TMPPTC=$(mktemp)

# get real load address
arm-none-eabi-readelf -s "$ELF"|grep -q __image_load_addr
if [ $? -eq 0 ]; then
  ADDR=$(printf "%x\n" 0x$(arm-none-eabi-readelf -s "$ELF"|grep __image_load_addr|grep GLOBAL|awk '{print $2}'))
else
  echo "Warning: no linker symbol '__image_load_addr' found, fallback to autodetect load address!"
  ADDR=$(printf "%x" $(arm-none-eabi-readelf -W -l "$ELF" | awk '$1=="LOAD" && $8 ~ /E/ { print $3; exit }'))
fi

# get flat binary image
arm-none-eabi-objcopy -O binary --change-addresses -0x${ADDR} --gap-fill 0xff "$ELF" "$TMPOUT"
[ $? -eq 0 ] || { echo "Cannot create bin"; exit 1; }

# get length
LENGTH=$(stat -c%s "$TMPOUT")
LENGTH1=$(printf "%02x" $(( $LENGTH >> 24 & 0xff )))
LENGTH2=$(printf "%02x" $(( $LENGTH >> 16 & 0xff )))
LENGTH3=$(printf "%02x" $(( $LENGTH >>  8 & 0xff )))
LENGTH4=$(printf "%02x" $(( $LENGTH >>  0 & 0xff )))
echo "${LENGTH4}${LENGTH3}${LENGTH2}${LENGTH1}" | xxd -r -p. >"$TMPPTC"

# get load addr
LOAD=$(printf "%d" 0x$ADDR)
LOAD1=$(printf "%02x" $(( $LOAD >> 24 & 0xff )))
LOAD2=$(printf "%02x" $(( $LOAD >> 16 & 0xff )))
LOAD3=$(printf "%02x" $(( $LOAD >>  8 & 0xff )))
LOAD4=$(printf "%02x" $(( $LOAD >>  0 & 0xff )))
echo "${LOAD4}${LOAD3}${LOAD2}${LOAD1}" | xxd -r -p. >>"$TMPPTC"

# sha256
sha256sum -b "$TMPOUT" | xxd -r -p. >"$TMPSHA"
[ $? -eq 0 ] || { echo "Cannot make hash"; exit 1; }

# sign
openssl pkeyutl -sign -inkey "$KEY" -in "$TMPSHA" -out "$TMPDER"
[ $? -eq 0 ] || { echo "Signing failed"; exit 1; }

# der -> raw INTEGER #1
S=$(openssl asn1parse -in "$TMPDER" -inform DER|grep INTEGER|head -1|sed 's/.*://g')
[ $? -eq 0 ] || { echo "Signature export failed"; exit 1; }

if [ ${#S} -gt 64 ]; then
  S=${S: -64}
elif [ ${#S} -lt 64 ]; then
  S=$(head -c $((64-${#S})) < /dev/zero | tr '\0' '0')$S
fi
echo -n "$S"|xxd -r -p.  >"$TMPSIG"

# der -> raw INTEGER #1
S=$(openssl asn1parse -in "$TMPDER" -inform DER|grep INTEGER|tail -1|sed 's/.*://g')
[ $? -eq 0 ] || { echo "Signature export failed"; exit 1; }
if [ ${#S} -gt 64 ]; then
  S=${S: -64}
elif [ ${#S} -lt 64 ]; then
  S=$(head -c $((64-${#S})) < /dev/zero | tr '\0' '0')$S
fi
echo -n "$S"|xxd -r -p. >>"$TMPSIG"

# add signature to patch file
cat "$TMPSIG" >>"$TMPPTC"

# search for header start
LC_ALL=C grep -aboq "RKKP" "$TMPOUT"
[ $? -eq 0 ] || { echo "Header not found in $ELF"; exit 1; }

# get patch location based on header struct
HDROFF=$(LC_ALL=C grep -abo "RKKP" "$TMPOUT"|head -1|sed 's/:.*//g')
if [ $HDROFF -gt 4096 ]; then
  echo "Header not found in the first 4k"
  exit 1
fi
PATCHOFFSET=$(($HDROFF + 16)) # based on pinekicker.h struct slot_header

# apply patchfile
dd if="$TMPPTC" of="$TMPOUT" bs=1 seek="$PATCHOFFSET" conv=notrunc status=none
[ $? -eq 0 ] || { echo "Patching binary failed"; exit 1; }

# rename result to final output
OUTNAME=$(dirname "$OUT")"/"$(basename "$OUT" .bin)"_0x"${ADDR}".bin"
mv "$TMPOUT" "$OUTNAME"

# remove temps
rm -f "$TMPSHA" "$TMPDER" "$TMPSIG" "$TMPOUT" "$TMPPTC"
