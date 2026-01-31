#!/bin/bash

BL=$1
IM1=$2
IM2=$3

FLASH=$(mktemp)

# generate empty flash
dd if=/dev/zero bs=512k count=1 2>/dev/null | LC_CTYPE=C tr '\0' '\377' >"$FLASH"

# fill with bootloader
dd if="$BL" of="${BL}.bin" bs=1 skip=32 2>/dev/null
dd if="${BL}.bin" of="$FLASH" bs=1 conv=notrunc 2>/dev/null

# add slot 1 image
ADDR1=$(echo $(basename "$IM1")|sed -e 's/.*_//g'|sed -e 's/[.].*//g')
dd if="$IM1" of="$FLASH" bs=1 seek=$(($ADDR1)) conv=notrunc 2>/dev/null

# add slot 2 image
ADDR2=$(echo $(basename "$IM2")|sed -e 's/.*_//g'|sed -e 's/[.].*//g')
dd if="$IM2" of="$FLASH" bs=1 seek=$(($ADDR2)) conv=notrunc 2>/dev/null

# get slot 1 reset_handler address
slot_base=$((ADDR1))
header_base=$(dd if="$FLASH" bs=1 skip=$slot_base count=4096 2>/dev/null | grep -abo $'\x52\x4b\x4b\x50' | head -n1 | cut -d: -f1)
vtor=$(dd if="$FLASH" bs=1 skip=$((slot_base + header_base + 8)) count=4 2>/dev/null | od -An -t u4 | tr -d ' ')
RH1=$(dd if="$FLASH" bs=1 skip=$((slot_base + vtor + 4)) count=4 2>/dev/null | od -An -t u4 | tr -d ' ')
RH1=$((RH1 & ~1))

# get slot 2 reset_handler address
slot_base=$((ADDR2))
header_base=$(dd if="$FLASH" bs=1 skip=$slot_base count=4096 2>/dev/null | grep -abo $'\x52\x4b\x4b\x50' | head -n1 | cut -d: -f1)
vtor=$(dd if="$FLASH" bs=1 skip=$((slot_base + header_base + 8)) count=4 2>/dev/null | od -An -t u4 | tr -d ' ')
RH2=$(dd if="$FLASH" bs=1 skip=$((slot_base + vtor + 4)) count=4 2>/dev/null | od -An -t u4 | tr -d ' ')
RH2=$((RH2 & ~1))

# start qemu with the flash content
QOUT=$(mktemp)
qemu-system-arm -M mps2-an386 -cpu cortex-m4 -nographic -S -gdb tcp::3333 -device loader,file="$FLASH",addr=0x00000000 2>"$QOUT" &
QEMU_PID=$!
sleep 1
TMP=$(mktemp)
cat >"$TMP" <<EOF
break *0x$(printf "%x" "$RH1")
break *0x$(printf "%x" "$RH2")

commands 1
  printf "BLTST: SLOT 1\n"
  monitor quit
end

commands 2
  printf "BLTST: SLOT 2\n"
  monitor quit
end

continue
EOF

# run gdb over it with timeout
timeout 5s arm-none-eabi-gdb -q -ex "target remote :3333" -x "$TMP" --batch 2>/dev/null | grep "^BLTST"

# cleanup
rm -f "$TMP" "$QOUT" "$FLASH"
kill $QEMU_PID 2>/dev/null
kill -9 $QEMU_PID 2>/dev/null
echo "DONE"
