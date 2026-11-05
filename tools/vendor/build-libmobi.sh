#!/bin/sh
set -eu
base=$(CDPATH='' cd "$(dirname "$0")/../.." && pwd)
: "${LIBMOBI:?Set LIBMOBI to extracted libmobi-0.12 sources}"
: "${ZIG:?Set ZIG to Zig 0.14.1 executable}"
: "${MOBI_BUILD:?Set MOBI_BUILD to an intermediate object directory}"
# Generate config.h first in LIBMOBI using:
# ./configure --with-libxml2=no --with-zlib=no --enable-encryption --disable-shared --enable-tools-static
# This configuration uses the bundled XML writer and miniz, device-serial decryption enabled.
zig="$ZIG"
mkdir -p "$MOBI_BUILD" "$base/build/kindle"
objects=""
for name in buffer compression debug index memory meta parse_rawml read structure util write opf xmlwriter encryption sha1 randombytes; do
 "$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -Os -Wno-date-time -DHAVE_CONFIG_H -I"$LIBMOBI"/src -c "$LIBMOBI/src/$name.c" -o "$MOBI_BUILD/$name.o"
 objects="$objects $MOBI_BUILD/$name.o"
done
"$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -Os -Wno-date-time -DHAVE_CONFIG_H -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -I"$LIBMOBI"/src -c "$LIBMOBI"/src/miniz.c -o "$MOBI_BUILD"/miniz.o
objects="$objects $MOBI_BUILD/miniz.o"
for name in common mobitool; do
 "$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -Os -Wno-date-time -DHAVE_CONFIG_H -I"$LIBMOBI"/src -c "$LIBMOBI/tools/$name.c" -o "$MOBI_BUILD/$name.o"
 objects="$objects $MOBI_BUILD/$name.o"
done
"$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -s $objects -lm -o "$base/build/kindle/mobitool"
