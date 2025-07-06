#!/bin/sh

t=$(date +'%Y/%m/%d %H:%M:%S')
d=$(date +'%Y/%m/%d')

rmj=1
rmi=$(git rev-list HEAD --count)

cat > vers.c << EOF
#include <efi.h>
#include <efilib.h>

const CHAR16 version[] = L"Version 1.0";
const CHAR16 revision[] = L"Internal revision ${rmj}.${rmi}, ${d}";
const CHAR16 blddate[] = L"${t}";
EOF
