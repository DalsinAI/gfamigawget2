#!/bin/sh
# PC test for scripts/write-part.sh: the Part file GFOpenUpTool reads.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
mkdir -p "$T/C" "$T/Libs" "$T/Docs/Wget2"
: > "$T/C/wget2"; : > "$T/Libs/regex.library"; : > "$T/Docs/Wget2/ReadMe"; : > "$T/Docs/Wget2/COPYING-Wget2"
sh "$HERE/../scripts/write-part.sh" "$T" 2.3.0
fail=0
expect() { grep -qxF "$1" "$T/Part" || { echo "FAIL: no line: $1"; fail=1; }; }
expect 'part wget2 2.3.0'
expect 'needs 68040'
expect 'copy "C/wget2" "SYS:C/wget2"'
expect 'copy "Docs/Wget2/ReadMe" "SYS:Docs/Wget2/ReadMe"'
expect 'copy "Docs/Wget2/COPYING-Wget2" "SYS:Docs/Wget2/COPYING-Wget2"'
expect 'add "Libs/regex.library" "LIBS:regex.library"'
[ "$(head -1 "$T/Part")" = 'part wget2 2.3.0' ] || { echo "FAIL: first line"; fail=1; }
[ "$(grep -c '^copy' "$T/Part")" = 3 ] || { echo "FAIL: file count (the Part file itself must not be listed)"; fail=1; }
sh "$HERE/../scripts/write-part.sh" "$T" 2.3.0   # a second run: the same file
[ "$(grep -c '^copy\|^add' "$T/Part")" = 4 ] || { echo "FAIL: second run lists the Part file"; fail=1; }
[ $fail = 0 ] && echo "Part file: all checks passed"
exit $fail
