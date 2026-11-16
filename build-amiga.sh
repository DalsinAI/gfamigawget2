#!/bin/sh
# Builds GNU Wget2, the downloader, for GFOS (AmigaOS 3.2.3 with GFOpenUp)
# with a stove: 68040, soft float with the FPU taken when there is one
# (fpuauto), sockets through libnix's socket layer on bsdsocket.library,
# HTTPS through GFOpenTLS (gfopentls.library, amiga/ssl_gfopentls.c), and
# regular expressions (--accept-regex, --reject-regex) through GFOpenRegex
# (regex.library).
#   STOVE=.../stoves/os32-gcc16-open6 OUT=out ./build-amiga.sh
# Wget2, GFOpenRegex, GFOpenTLS's headers and fpuauto are fetched as
# wget2.lock gives; nothing third-party is kept here. WGET2_SRC=... takes a
# Wget2 tree already on disk. The PC tests (tests/host-wget2.sh: the same
# sources, settings and TLS code, real downloads from local HTTP and HTTPS
# servers) run first when there is a PC compiler; HOST_TESTS=0 skips them.
# NDK_INCLUDE=... adds an OS header folder after the stove's own (a stove
# that has none, such as the bare gfopengcc release with GFOpenSDK's
# OpenNDK/Include_H).
# OUT gets Parts/wget2: a GFOpenUp part (its Part file, C:wget2,
# LIBS:regex.library, the manual and licences in SYS:Docs/Wget2), installed
# with
#   GFOpenUpTool INSTALL Parts/wget2
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
. "$HERE/wget2.lock"
. "$HERE/scripts/wget2-common.sh"
STOVE=${STOVE:-"$HOME/AmiCradle/stoves/os32-gcc16-open6"}
# outside the checkout: Cradle cleans it (git clean -ffdx) before every job
WORK="${WORK:-$HOME/.cache/gfamigawget2/build}"; OUT="${OUT:-$WORK/out}"; JOBS=${JOBS:-8}
TC="$STOVE/prefix"; PATH="$TC/bin:$PATH"; export PATH
mkdir -p "$WORK/deps" "$OUT"; OUT=$(cd "$OUT" && pwd); WORK=$(cd "$WORK" && pwd)
NDK=""; [ -z "${NDK_INCLUDE:-}" ] || NDK="-idirafter $(cd "$NDK_INCLUDE" && pwd)"
t0=$(date +%s)

# --- Wget2 at its release, with patches/ ------------------------------------
SRC="$WORK/wget2"
wget2_source "$SRC"

# --- GFOpenTLS: gfopentls.library's headers (and the PC tests' TLS) ----------
if [ -z "${OT_SRC:-}" ]; then fetch_at "$WORK/deps/gfopentls" "$OT_REPO" "$OT_COMMIT"; OT_SRC="$WORK/deps/gfopentls"; fi

# --- the PC tests first: the same sources and settings ------------------------
if [ "${HOST_TESTS:-1}" != 0 ] && command -v "${HOST_CC:-cc}" > /dev/null 2>&1; then
    WORK="$WORK/host" OT_SRC="$OT_SRC" sh "$HERE/tests/host-wget2.sh" "$SRC" > "$WORK/host-tests.log" 2>&1 || { grep -E "passed|FAIL|^  " "$WORK/host-tests.log" | tail -40; exit 1; }
    grep -E "all checks passed" "$WORK/host-tests.log"
fi

# --- GFOpenRegex: regex.library and its link stub -----------------------------
if [ -z "${RX_SRC:-}" ]; then fetch_at "$WORK/deps/gfopenregex" "$RX_REPO" "$RX_COMMIT"; RX_SRC="$WORK/deps/gfopenregex"; fi
RXB="$WORK/gfopenregex"
if [ ! -f "$RXB/lib/libregex.a" ] || [ "$(cat "$RXB/.rev" 2>/dev/null)" != "$RX_COMMIT" ]; then
    rm -rf "$RXB"
    STOVE="$STOVE" OUT="$RXB" HOST_TESTS=0 NDK_INC="${NDK_INCLUDE:-}" sh "$RX_SRC/build-amiga.sh" > "$WORK/regex.log" 2>&1 || { tail -30 "$WORK/regex.log"; exit 1; }
    echo "$RX_COMMIT" > "$RXB/.rev"
fi

# --- fpuauto: libfpuauto.a and its link specs ---------------------------------
FPD="$WORK/deps/fpuauto"
if [ "$(git -C "$FPD" rev-parse HEAD 2>/dev/null)" != "$FPA_COMMIT" ]; then
    rm -rf "$FPD"; mkdir -p "$FPD"; git -C "$FPD" init -q
    git -C "$FPD" config core.sparseCheckout true
    echo "/common/fpuauto/" > "$FPD/.git/info/sparse-checkout"
    git -C "$FPD" fetch -q --depth 1 --filter=blob:none "$FPA_REPO" "$FPA_COMMIT"
    git -C "$FPD" checkout -q FETCH_HEAD
fi
FA="$FPD/common/fpuauto"; FAL="$WORK/fpuauto"
if [ ! -f "$FAL/libfpuauto.a" ] || [ "$(cat "$FAL/.rev" 2>/dev/null)" != "$FPA_COMMIT" ]; then
    rm -rf "$FAL"; mkdir -p "$FAL/obj"
    m68k-amigaos-gcc -m68040 -DFPUAUTO_COMMON -c "$FA/fpuauto.S" -o "$FAL/obj/fa_common.o"
    for n in $(sed 's/^--wrap=//' "$FA/fpuauto.wraps"); do
        m68k-amigaos-gcc -m68040 -DR_$n -c "$FA/fpuauto.S" -o "$FAL/obj/fa_$n.o"
    done
    m68k-amigaos-ar rcs "$FAL/libfpuauto.a" "$FAL"/obj/fa_*.o
    echo "$FPA_COMMIT" > "$FAL/.rev"
fi

# --- configure's answers for the Amiga ------------------------------------------
BLD="$WORK/b68k"
# shellcheck disable=SC2086
GFOS=1 wget2_config "$SRC" "$BLD" --host=m68k-amigaos --build="$(sh "$SRC/build-aux/config.guess")" \
    CC="m68k-amigaos-gcc -noixemul -m68040" CFLAGS="-O2 $NDK" LIBS="-lsocket" $WGET2_CONFIGURE
echo "configure: $(grep -c '^#define HAVE_' "$BLD/gf/config.h") answers"

# --- compile -------------------------------------------------------------------
LATE=""
if m68k-amigaos-gcc -fno-late-combine-instructions -S -o /dev/null -x c /dev/null 2>/dev/null; then LATE="-fno-late-combine-instructions"; fi
# no loops turned into memset/memcpy calls (GCC 16), address 0 is memory
CC=m68k-amigaos-gcc
CF="-noixemul -m68040 -O2 -fno-tree-loop-distribute-patterns -fno-delete-null-pointer-checks $LATE $NDK \
 -Werror=implicit-function-declaration -Werror=int-conversion -Wno-cpp \
 -DHAVE_CONFIG_H -DSYSCONFDIR=\\\"ENVARC:Wget2/\\\" -DLOCALEDIR=\\\"LOCALE:\\\" \
 -DGFINET_NEED_SOCKADDR_STORAGE -DGFINET_SIN_LEN -I$RXB/include -DGFWGET_DATE=\\\"$(date -u +%d.%m.%Y)\\\""
O="$WORK/obj"
[ "$(cat "$O/.src" 2>/dev/null)" = "$(cat "$SRC/.patched")" ] || { rm -rf "$O"; mkdir -p "$O"; cat "$SRC/.patched" > "$O/.src"; }
WGET2_OURS="gfwget_compat gfinet amiga_version" wget2_makefile "$O" "$SRC" "$BLD"
(cd "$O" && nice -n 19 make -k -j"$JOBS" all TLS_CF="-I$OT_SRC/include" > make.log 2>&1) || { grep -E -B3 'error' "$O/make.log" | grep -v '^m68k-amigaos-gcc' | head -60; exit 1; }
echo "compile: $(( $(date +%s) - t0 )) s"

# --- link: soft-float specs, then fpuauto's; libnix's socket layer, regex.library's stub
LINK="m68k-amigaos-gcc -noixemul -m68040 -specs=$HERE/scripts/m68k-soft-m68040.specs -specs=$FA/fpuauto.specs -L$FAL"
# shellcheck disable=SC2046
$LINK -o "$O/wget2.debug" $(cd "$O" && for f in $(sed -n 's/^OBJS =//p' Makefile); do echo "$O/$f"; done) \
    -L"$RXB/lib" -lregex -lsocket -lm > "$O/link.log" 2>&1 || { head -60 "$O/link.log"; exit 1; }

# --- the GFOpenUp part: Parts/wget2 ----------------------------------------------
P="$OUT/Parts/wget2"; rm -rf "$P"; mkdir -p "$P/C" "$P/Libs" "$P/Docs/Wget2"
m68k-amigaos-strip -o "$P/C/wget2" "$O/wget2.debug"
cp "$RXB/lib/regex.library" "$P/Libs/regex.library"
cp "$HERE/README.md" "$P/Docs/Wget2/ReadMe"
cp "$SRC/COPYING" "$P/Docs/Wget2/COPYING-Wget2"
[ ! -f "$SRC/docs/wget2.md" ] || cp "$SRC/docs/wget2.md" "$P/Docs/Wget2/wget2.md"
cp "$RX_SRC/LICENSE" "$P/Docs/Wget2/LICENSE-GFOpenRegex"
cp "$HERE/LICENSE" "$P/Docs/Wget2/LICENSE-port"
sh "$HERE/scripts/write-part.sh" "$P" "$WGET2_VERSION"
ls -l "$P/C/wget2" "$P/Libs/regex.library"
echo "part: $(grep -c '^copy\|^add' "$P/Part") files, $(du -sk "$P" | cut -f1) KB"
echo "fpuauto: $(m68k-amigaos-nm "$O/wget2.debug" | grep -c ' T ___wrap_') wrapped routines"
echo "total: $(( $(date +%s) - t0 )) s"
