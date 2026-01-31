#!/bin/bash

KEY=$1
HDR=$2

if [ x"$2" == x"" ]; then
  echo "Usage: $0 keyname header.h"
  exit 1
fi

openssl ecparam -name prime256v1 -genkey -noout -out "$KEY.pem"
echo "#ifndef PUBKEY_H" >"$HDR"
echo "#define PUBKEY_H" >>"$HDR"
openssl ec -in "$KEY.pem" -pubout -text -noout 2>/dev/null|grep ^pub -A5|tail +2|tr ':' '\n'|grep -v ^$|awk '{print $1}'|tail +2|awk 'BEGIN {printf "unsigned char pubkey[]={";} // {printf "0x" $1 "," } END {printf "}\n"} ' >>"$HDR"
echo "#endif" >>"$HDR"
