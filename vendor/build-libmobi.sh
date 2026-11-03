#!/bin/sh
set -eu
base=$(CDPATH='' cd "$(dirname "$0")/.." && pwd)
: "${LIBMOBI:?Set LIBMOBI to extracted libmobi-0.12 sources}"
: "${ZIG:?Set ZIG to Zig 0.14.1 executable}"
: "${MOBI_BUILD:?Set MOBI_BUILD to an intermediate object directory}"
# Generate config.h first in LIBMOBI using:
# ./configure --with-libxml2=no --with-zlib=no --disable-encryption --disable-shared --enable-tools-static
# This configuration uses the bundled XML writer and miniz, no DRM code.
zig="$ZIG"
mkdir -p "$MOBI_BUILD" "$base/build/kindle"
for name in buffer compression debug index memory meta parse_rawml read structure util write opf xmlwriter; do
 "$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -Os -Wno-date-time -DHAVE_CONFIG_H -I"$LIBMOBI"/src -c "$LIBMOBI/src/$name.c" -o "$MOBI_BUILD/$name.o"
done
"$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -Os -Wno-date-time -DHAVE_CONFIG_H -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -I"$LIBMOBI"/src -c "$LIBMOBI"/src/miniz.c -o "$MOBI_BUILD"/miniz.o
for name in common mobitool; do
 "$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -Os -Wno-date-time -DHAVE_CONFIG_H -I"$LIBMOBI"/src -c "$LIBMOBI/tools/$name.c" -o "$MOBI_BUILD/$name.o"
done
"$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -s "$MOBI_BUILD"/*.o -lm -o "$base/build/kindle/mobitool"
