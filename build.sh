#!/bin/sh
# Builds TraySense.exe and the installer, TraySense-Setup.exe, which has TraySense.exe inside.
#
# Needs MinGW-w64 (gcc + windres). On Windows the easiest is w64devkit
# (https://github.com/skeeto/w64devkit): open w64devkit.exe, go to this folder, run "sh build.sh".
# GitHub builds every release with this same script (see .github/workflows/build.yml).
set -e
cd "$(dirname "$0")"

CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
command -v "$CC" >/dev/null 2>&1 || { CC=gcc; WINDRES=windres; }
"$CC" --version | head -n 1

# The installer includes the official PawnIO 2.2.0 setup (the CPU temperature driver).
# It is downloaded from PawnIO's own GitHub release and only used if its SHA-256 matches.
PAWN=src/installer/pawnio/PawnIO_setup.exe
PAWN_URL=https://github.com/namazso/PawnIO.Setup/releases/download/2.2.0/PawnIO_setup.exe
PAWN_SHA256=1f519a22e47187f70a1379a48ca604981c4fcf694f4e65b734aaa74a9fba3032
if [ ! -f "$PAWN" ]; then
    mkdir -p src/installer/pawnio
    curl -fsSL -o "$PAWN" "$PAWN_URL"
fi
echo "$PAWN_SHA256  $PAWN" | sha256sum -c -

# 1. TraySense.exe
cd src/TraySense-main
"$WINDRES" traysense.rc -O coff -o traysense.res
"$CC" -Os -s -mwindows -Wall -ffunction-sections -fdata-sections -Wl,--gc-sections \
      traysense.c traysense.res -o TraySense.exe -lole32 -loleaut32

# 2. TraySense-Setup.exe
cd ../installer
"$WINDRES" setup.rc -O coff -o setup.res
"$CC" -Os -s -municode -mwindows -Wall -Wextra \
      setup.c setup.res -o TraySense-Setup.exe -lole32 -luuid -lshell32 -ladvapi32 -lcomctl32 -lbcrypt -lsetupapi

cd ../..
cp src/installer/TraySense-Setup.exe .
ls -l TraySense-Setup.exe
sha256sum TraySense-Setup.exe
