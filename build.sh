#!/bin/sh
# Builds TrayVitals.exe and the installer, TrayVitals-Setup.exe, which has TrayVitals.exe inside.
#
# Needs MinGW-w64 (gcc + windres). On Windows the easiest is w64devkit
# (https://github.com/skeeto/w64devkit): open w64devkit.exe, go to this folder, run "sh build.sh".
# GitHub builds every release with this same script (see .github/workflows/build.yml).
#
# Your own build options go in EXTRA_CFLAGS, for example:
#   EXTRA_CFLAGS='-DTV_UPDATE_REPO=L""' sh build.sh                 no update checks
#   EXTRA_CFLAGS='-DTV_UPDATE_REPO=L"you/your-fork"' sh build.sh    updates from your own fork
#   EXTRA_CFLAGS='-DTV_MENU_MODERN=0' sh build.sh                    Windows' menu by default
set -e
cd "$(dirname "$0")"

CC=${CC:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
command -v "$CC" >/dev/null 2>&1 && command -v "$WINDRES" >/dev/null 2>&1 || { CC=gcc; WINDRES=windres; }
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

# 1. TrayVitals.exe
cd src/TrayVitals
"$WINDRES" trayvitals.rc -O coff -o trayvitals.res
"$CC" -Os -s -mwindows -Wall -ffunction-sections -fdata-sections -Wl,--gc-sections $EXTRA_CFLAGS \
      trayvitals.c trayvitals.res -o TrayVitals.exe -lole32 -loleaut32

# 2. TrayVitals-Setup.exe
cd ../installer
"$WINDRES" setup.rc -O coff -o setup.res
"$CC" -Os -s -municode -mwindows -Wall -Wextra \
      setup.c setup.res -o TrayVitals-Setup.exe -lole32 -luuid -lshell32 -ladvapi32 -lcomctl32 -lbcrypt -lsetupapi

cd ../..
cp src/installer/TrayVitals-Setup.exe .
ls -l TrayVitals-Setup.exe
sha256sum TrayVitals-Setup.exe
