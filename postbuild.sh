#!/bin/bash
set -e

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
  exit 0
fi

TMPSHA=$(mktemp)
TMPDER=$(mktemp)
TMPSIG=$(mktemp)
TMPOUT=$(mktemp)
TMPPTC=$(mktemp)

ADDR=$(printf "%x" $(arm-none-eabi-readelf -W -l "$ELF" | awk '$1=="LOAD" && $8 ~ /E/ { print $3; exit }'))
arm-none-eabi-objcopy -O binary --change-addresses -0x${ADDR} --gap-fill 0xff "$ELF" "$TMPOUT"
LENGTH=$(stat -c%s "$TMPOUT")
LENGTH1=$(printf "%02x" $(( $LENGTH >> 24 & 0xff )))
LENGTH2=$(printf "%02x" $(( $LENGTH >> 16 & 0xff )))
LENGTH3=$(printf "%02x" $(( $LENGTH >>  8 & 0xff )))
LENGTH4=$(printf "%02x" $(( $LENGTH >>  0 & 0xff )))
echo "${LENGTH4}${LENGTH3}${LENGTH2}${LENGTH1}" | xxd -r -p. >"$TMPPTC"
sha256sum -b "$TMPOUT" | xxd -r -p. >"$TMPSHA"
openssl pkeyutl -sign -inkey "$KEY" -in "$TMPSHA" -out "$TMPDER"

S=$(openssl asn1parse -in "$TMPDER" -inform DER|grep INTEGER|head -1|sed 's/.*://g')
if [ ${#S} -gt 64 ]; then
  S=${S: -64}
elif [ ${#S} -lt 64 ]; then
  S=$(head -c $((64-${#S})) < /dev/zero | tr '\0' '0')$S
fi
echo -n "$S"|xxd -r -p.  >"$TMPSIG"

S=$(openssl asn1parse -in "$TMPDER" -inform DER|grep INTEGER|tail -1|sed 's/.*://g')
if [ ${#S} -gt 64 ]; then
  S=${S: -64}
elif [ ${#S} -lt 64 ]; then
  S=$(head -c $((64-${#S})) < /dev/zero | tr '\0' '0')$S
fi
echo -n "$S"|xxd -r -p. >>"$TMPSIG"

cat "$TMPSIG" >>"$TMPPTC"
LC_ALL=C grep -aboq "RKKP" "$TMPOUT"
if [ $? -ne 0 ]; then
  echo "Header not found in $ELF"
  exit 1
fi
HDROFF=$(LC_ALL=C grep -abo "RKKP" "$TMPOUT"|head -1|sed 's/:.*//g')
if [ $HDROFF -gt 4096 ]; then
  echo "Header not found in the first 4k"
  exit 1
fi
PATCHOFFSET=$(($HDROFF + 32)) # based on pinekicker.h struct slot_header
dd if="$TMPPTC" of="$TMPOUT" bs=1 seek="$PATCHOFFSET" conv=notrunc status=none
mv "$TMPOUT" "$OUT"

rm -f "$TMPSHA" "$TMPDER" "$TMPSIG" "$TMPOUT" "$TMPPTC"
