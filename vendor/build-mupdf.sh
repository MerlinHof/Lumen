#!/bin/sh
set -eu
: "${MUPDF:?Set MUPDF to the extracted mupdf-1.28.5-source directory}"
cd "$MUPDF"
if [ "${1:-}" = host ]; then
 exec make -j4 libs OUT=build/lumen-host HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no mujs=no extract=no xps=no tofu_cjk=yes tofu_cjk_ext=yes XCFLAGS='-Os -DFZ_ENABLE_ICC=0'
else
 : "${ZIG:?Set ZIG to the Zig 0.14.1 executable}"
 exec make -j4 libs OUT=build/lumen-arm OS=Linux HAVE_OBJCOPY=no HAVE_X11=no HAVE_GLUT=no HAVE_CURL=no CC="$ZIG cc -target arm-linux-musleabi -mcpu=cortex_a9" CXX="$ZIG c++ -target arm-linux-musleabi -mcpu=cortex_a9" AR="$ZIG ar" RANLIB="$ZIG ranlib" mujs=no extract=no xps=no tofu_cjk=yes tofu_cjk_ext=yes XCFLAGS='-Os -DFZ_ENABLE_ICC=0' XCXXFLAGS='-fno-exceptions -fno-rtti'
fi
