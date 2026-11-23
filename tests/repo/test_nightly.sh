#!/bin/sh
# build-nightly.sh and autobuild.md: no NIGHTLY_OUT is refused, a missing open9 stove is refused, and with a
# made-up stove it hands build-amiga.sh the open9 stove, NIGHTLY_OUT as OUT and a work folder inside it.
#   sh tests/repo/test_nightly.sh
# MIT, Copyright (c) 2026 AmiCradle Team.
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
pass=0; fail=0
ok() { pass=$((pass + 1)); }
no() { fail=$((fail + 1)); echo "FAIL: $1"; }

# a copy of the checkout whose build-amiga.sh only says what it was given
mkdir -p "$T/repo"; cp "$HERE/build-nightly.sh" "$T/repo/"
cat > "$T/repo/build-amiga.sh" <<'X'
echo "STOVE=$STOVE"; echo "OUT=$OUT"; echo "WORK=$WORK"
X

out=$(env -u NIGHTLY_OUT sh "$T/repo/build-nightly.sh" 2>&1); rc=$?
[ $rc != 0 ] && ok || no "runs with no NIGHTLY_OUT"
echo "$out" | grep -q NIGHTLY_OUT && ok || no "does not name NIGHTLY_OUT: $out"

out=$(NIGHTLY_OUT="$T/out" STOVES="$T/none" sh "$T/repo/build-nightly.sh" 2>&1); rc=$?
[ $rc != 0 ] && ok || no "runs with no open9 stove"
echo "$out" | grep -q "no open9 stove" && ok || no "does not say the stove is missing: $out"

mkdir -p "$T/stoves/os32-gcc16-open9/prefix/bin"
printf '#!/bin/sh\necho "m68k-amigaos-gcc (GFOpenAmigaGCC 16.2.0-open9) 16.2.0"\n' > "$T/stoves/os32-gcc16-open9/prefix/bin/m68k-amigaos-gcc"
chmod +x "$T/stoves/os32-gcc16-open9/prefix/bin/m68k-amigaos-gcc"
out=$(NIGHTLY_OUT="$T/out" STOVES="$T/stoves" STOVE=/somewhere/os32-gcc16 sh "$T/repo/build-nightly.sh" 2>&1); rc=$?
[ $rc = 0 ] && ok || no "fails with a stove: $out"
echo "$out" | grep -qx "STOVE=$T/stoves/os32-gcc16-open9" && ok || no "not the open9 stove: $out"
echo "$out" | grep -qx "OUT=$T/out" && ok || no "OUT is not NIGHTLY_OUT: $out"
echo "$out" | grep -qx "WORK=$T/out/.work" && ok || no "WORK is not inside NIGHTLY_OUT: $out"
echo "$out" | grep -q "^Compiler: .*open9" && ok || no "compiler not shown: $out"

# autobuild.md names the script, a part, and keeps what build-amiga.sh makes
A="$HERE/autobuild.md"
grep -qx "script: build-nightly.sh" "$A" && ok || no "autobuild.md: script"
grep -qx "frequency: nightly" "$A" && ok || no "autobuild.md: frequency"
grep -qx "kind: part" "$A" && ok || no "autobuild.md: kind"
grep -qx "keep: out/Parts/wget2" "$A" && ok || no "autobuild.md: keep"
grep -q 'P="$OUT/Parts/wget2"' "$HERE/build-amiga.sh" && ok || no "build-amiga.sh does not make OUT/Parts/wget2"
[ -f "$HERE/$(sed -n 's/^script: //p' "$A")" ] && ok || no "the script autobuild.md names is not in the repository"

echo "test_nightly: $pass passed, $fail failed"
[ $fail = 0 ]
