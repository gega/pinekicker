#!/bin/bash

BL=$1
IM1=$2
IM2=$3


FLASH=$(mktemp)

dd if=/dev/zero bs=512k count=1 2>/dev/null | LC_CTYPE=C tr '\0' '\377' >"$FLASH"
dd if="$BL" of="${BL}.bin" bs=1 skip=32 2>/dev/null
dd if="${BL}.bin" of="$FLASH" bs=1 conv=notrunc 2>/dev/null
ADDR=$(echo $(basename "$IM1")|sed -e 's/.*_//g'|sed -e 's/[.].*//g')
echo $ADDR
dd if="$IM1" of="$FLASH" bs=1 seek=$(($ADDR)) conv=notrunc 2>/dev/null
ADDR=$(echo $(basename "$IM2")|sed -e 's/.*_//g'|sed -e 's/[.].*//g')
echo $ADDR
dd if="$IM2" of="$FLASH" bs=1 seek=$(($ADDR)) conv=notrunc 2>/dev/null

get_reset_handler() {
  local image_base=$1

  dd if="$FLASH" \
     bs=1 \
     skip=$((image_base + 4)) \
     count=4 2>/dev/null |
  od -An -t u4 |
  tr -d ' '
}

RH1=$(printf "%d" 0x8130)
RH2=$(printf "%d" 0x42130)
RH1=$((RH1 & ~1))
RH2=$((RH2 & ~1))

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
  quit
end

commands 2
  printf "BLTST: SLOT 2\n"
  quit
end

continue
EOF

timeout 5s arm-none-eabi-gdb -q -ex "target remote :3333" -x "$TMP" --batch 2>/dev/null | grep "^BLTST"

rm -f "$TMP" "$QOUT" "$FLASH"

kill $QEMU_PID 2>/dev/null

kill -9 $QEMU_PID 2>/dev/null

echo "DONE"
