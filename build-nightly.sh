#!/bin/sh
# The nightly build (autobuild.md): build-amiga.sh with the open9 stove, the part in $NIGHTLY_OUT/Parts/wget2.
# The nightly's own STOVE is the plain os32-gcc16 stove, so the open9 one is taken from STOVES here.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${NIGHTLY_OUT:?the nightly sets NIGHTLY_OUT}"
STOVE="${STOVES:-/srv/samsung/stoves}/os32-gcc16-open9"
[ -x "$STOVE/prefix/bin/m68k-amigaos-gcc" ] || { echo "build-nightly.sh: no open9 stove at $STOVE"; exit 1; }
echo "Compiler: $("$STOVE/prefix/bin/m68k-amigaos-gcc" --version | head -n 1)"
STOVE="$STOVE" OUT="$NIGHTLY_OUT" WORK="$NIGHTLY_OUT/.work" exec sh "$HERE/build-amiga.sh"
