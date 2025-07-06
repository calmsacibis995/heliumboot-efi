#!/bin/sh

#
# Use C locale for the date command, regardless of the user's locale.
# This way, the date will be in English and not in the user's language,
# if it doesn't happen to be English.
#
LC_TIME=C

t=$(date +'%Y/%m/%d %H:%M:%S')
d=$(date +'%Y/%m/%d')
ti=$(date)

rmj=1
rmi=$(git rev-list HEAD --count)

cat > vers.c << EOF
#include <efi.h>
#include <efilib.h>

const CHAR16 version[] = L"Version 1.0";
const CHAR16 revision[] = L"Internal revision ${rmj}.${rmi}, ${d}";
const CHAR16 blddate[] = L"${ti}";
EOF
