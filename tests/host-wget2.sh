#!/bin/sh
# The PC tests: what build-amiga.sh builds for the Amiga, built and run on
# the PC. Wget2's own files with the same configure answers (config.h
# without its replacement names), the same file lists and the same TLS
# backend (amiga/ssl_gfopentls.c) over GFOpenTLS's host build, at the
# commit wget2.lock gives. Then:
#   - our compat calls held to glibc's (tests/compat_test.c), gfinet's
#     name and address calls (tests/gfinet_test.c), the Part file
#     (tests/test-part.sh)
#   - real downloads from local servers (python3): a page, a site with -r,
#     -N dates, --accept-regex, HTTPS through GFOpenTLS with a test CA,
#     a wrong CA and a wrong host name refused, --no-check-certificate,
#     and a second HTTPS connection resuming the first one's session
#   host-wget2.sh [patched Wget2 source] (build-amiga.sh passes its own;
#   without one, Wget2 is fetched and patched here, WGET2_SRC=... as in
#   build-amiga.sh)
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$HERE/wget2.lock"
. "$HERE/scripts/wget2-common.sh"
WORK=${WORK:-"$HOME/.cache/gfamigawget2/host"}
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
SRC=${1:-}
if [ -z "$SRC" ]; then SRC="$WORK/wget2"; wget2_source "$SRC"; fi
CC=${HOST_CC:-cc}
fails=0
ok() { echo "  ok   $1"; }
bad() { echo "  FAIL $1"; fails=$((fails + 1)); }

# --- our compat and gfinet, held to glibc ---------------------------------------
echo "compat, gfinet, Part file:"
$CC -O1 -Wall -Wextra -DGFWGET_HOST_TEST -I"$HERE/amiga" -o "$WORK/compat_test" \
    "$HERE/tests/compat_test.c" "$HERE/amiga/gfwget_compat.c"
$CC -O1 -Wall -Wextra -I"$HERE/amiga" -o "$WORK/gfinet_test" \
    "$HERE/tests/gfinet_test.c" "$HERE/amiga/gfinet.c"
(cd "$WORK" && ./compat_test) || fails=$((fails + 1))
"$WORK/gfinet_test" || fails=$((fails + 1))
sh "$HERE/tests/test-part.sh" || fails=$((fails + 1))

# --- GFOpenTLS's host libraries at OT_COMMIT -------------------------------------
if [ -z "${OT_SRC:-}" ]; then fetch_at "$WORK/deps/gfopentls" "$OT_REPO" "$OT_COMMIT"; OT_SRC="$WORK/deps/gfopentls"; fi
OTB="$WORK/gfopentls"
if [ ! -f "$OTB/libopentls.a" ] || [ "$(cat "$OTB/.rev" 2>/dev/null)" != "$OT_COMMIT" ]; then
    rm -rf "$OTB"; mkdir -p "$OTB"
    (cd "$OTB" && cmake -S "$OT_SRC" -B . -DCMAKE_BUILD_TYPE=Release > cmake.log 2>&1 \
        && make -j"${JOBS:-8}" opentls bearssl opencrypto > make.log 2>&1) || { tail -20 "$OTB/make.log"; exit 1; }
    echo "$OT_COMMIT" > "$OTB/.rev"
fi

# --- Wget2 for the PC: configure's answers, our file lists, our TLS -----------------
BLD="$WORK/bpc"
# shellcheck disable=SC2086
GFOS=0 wget2_config "$SRC" "$BLD" CC="$CC" CFLAGS="-O2" $WGET2_CONFIGURE
CF="-O2 -DHAVE_CONFIG_H -DSYSCONFDIR=\\\"/nonexistent/\\\" -DLOCALEDIR=\\\"/nonexistent\\\" -DGFWGET_HOST_TEST -Wno-cpp \
 -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion"
O="$WORK/obj"
[ "$(cat "$O/.src" 2>/dev/null)" = "$(cat "$SRC/.patched")" ] || { rm -rf "$O"; mkdir -p "$O"; cat "$SRC/.patched" > "$O/.src"; }
WGET2_OURS="gfwget_compat" wget2_makefile "$O" "$SRC" "$BLD"
(cd "$O" && make -k -j"${JOBS:-8}" all TLS_CF="-I$OT_SRC/host/include -I$OT_SRC/include" > make.log 2>&1) \
    || { grep -E -B3 'error' "$O/make.log" | grep -v "^$CC" | head -40; exit 1; }
# shellcheck disable=SC2046
$CC -o "$WORK/wget2" $(cd "$O" && for f in $(sed -n 's/^OBJS =//p' Makefile); do echo "$O/$f"; done) \
    "$OTB/libopentls.a" "$OTB/libbearssl.a" "$OTB/libopencrypto.a" -lm > "$O/link.log" 2>&1 || { head -40 "$O/link.log"; exit 1; }
echo "built $WORK/wget2"

# --- real downloads ---------------------------------------------------------------
T="$WORK/run"; rm -rf "$T"; mkdir -p "$T/site/sub" "$T/empty"
python3 "$HERE/tests/make_site.py" "$T/site"
openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj "/CN=GF Test CA" \
    -keyout "$T/ca.key" -out "$T/ca.pem" > /dev/null 2>&1
openssl req -newkey rsa:2048 -nodes -subj "/CN=localhost" -keyout "$T/srv.key" -out "$T/srv.csr" > /dev/null 2>&1
printf 'subjectAltName=DNS:localhost\n' > "$T/ext.cnf"
openssl x509 -req -in "$T/srv.csr" -CA "$T/ca.pem" -CAkey "$T/ca.key" -CAcreateserial -days 30 \
    -extfile "$T/ext.cnf" -out "$T/srv.pem" > /dev/null 2>&1
openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj "/CN=Other CA" \
    -keyout "$T/other.key" -out "$T/other.pem" > /dev/null 2>&1
python3 "$HERE/tests/serve.py" "$T/site" "$T/srv.pem" "$T/srv.key" "$T/ports" > "$T/server.log" 2>&1 &
SP=$!
trap 'kill $SP 2>/dev/null || true' EXIT
i=0; while [ ! -s "$T/ports" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
read -r HP SP2 < "$T/ports"
W="$WORK/wget2 --no-config --tries=1 --timeout=10 --no-hsts --no-netrc --no-cookies --hsts-file=/dev/null"
echo "downloads:"

cd "$T" && mkdir -p d1 && cd d1
if $W -q "http://127.0.0.1:$HP/index.html" && cmp -s index.html ../site/index.html; then ok "one page over HTTP"; else bad "one page over HTTP"; fi
cd "$T" && mkdir -p d2 && cd d2
if $W -q -r -nH "http://127.0.0.1:$HP/index.html" && cmp -s sub/big.bin ../site/sub/big.bin \
    && cmp -s sub/page2.html ../site/sub/page2.html && cmp -s style.css ../site/style.css \
    && cmp -s sub/from-css.png ../site/sub/from-css.png; then ok "a site with -r (links in HTML and CSS)"; else bad "a site with -r"; fi
cd "$T" && mkdir -p d3 && cd d3
if $W -q -N "http://127.0.0.1:$HP/sub/big.bin" \
    && [ "$(stat -c %Y big.bin)" = "$(stat -c %Y ../site/sub/big.bin)" ]; then ok "-N: the server's date on the file"; else bad "-N date"; fi
cd "$T" && mkdir -p d4 && cd d4
$W -q -r -nH --accept-regex '.*\.(html|bin)$' "http://127.0.0.1:$HP/index.html" || true
if [ -f sub/big.bin ] && [ ! -f style.css ]; then ok "--accept-regex"; else bad "--accept-regex"; fi
cd "$T" && mkdir -p d5 && cd d5
if $W -q --ca-directory="$T/empty" --ca-certificate="$T/ca.pem" "https://localhost:$SP2/sub/big.bin" \
    && cmp -s big.bin ../site/sub/big.bin; then ok "HTTPS through GFOpenTLS, 1 MB"; else bad "HTTPS"; fi
cd "$T" && mkdir -p d6 && cd d6
if ! $W --ca-directory="$T/empty" --ca-certificate="$T/other.pem" "https://localhost:$SP2/index.html" > err.txt 2>&1 \
    && grep -q "certificate not accepted: .*not signed by a trusted" err.txt && [ ! -f index.html ]; then
    ok "a certificate from another CA is refused"
else
    bad "a certificate from another CA is refused"; cat err.txt
fi
cd "$T" && mkdir -p d7 && cd d7
if ! $W --ca-directory="$T/empty" --ca-certificate="$T/ca.pem" "https://127.0.0.1:$SP2/index.html" > err.txt 2>&1 \
    && grep -q "certificate not accepted: .*for localhost, not 127.0.0.1" err.txt && [ ! -f index.html ]; then
    ok "a wrong host name is refused"
else
    bad "a wrong host name is refused"; cat err.txt
fi
cd "$T" && mkdir -p d8 && cd d8
if $W -q --no-check-certificate --ca-directory="$T/empty" "https://127.0.0.1:$SP2/index.html" \
    && cmp -s index.html ../site/index.html; then ok "--no-check-certificate"; else bad "--no-check-certificate"; fi
cd "$T" && mkdir -p d9 && cd d9
$W -d --no-http-keep-alive --ca-directory="$T/empty" --ca-certificate="$T/ca.pem" \
    "https://localhost:$SP2/index.html" "https://localhost:$SP2/style.css" > log.txt 2>&1 || true
if [ "$(grep -c 'Handshake completed' log.txt)" -ge 2 ] && grep -q 'Handshake completed (resumed session)' log.txt \
    && cmp -s style.css ../site/style.css; then ok "the second connection resumes the first one's session"; else bad "session resumption"; fi
cd "$T"

if [ "$fails" = 0 ]; then
    echo "all checks passed"
else
    echo "$fails checks FAILED"; exit 1
fi
