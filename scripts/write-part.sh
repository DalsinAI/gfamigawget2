#!/bin/sh
# Writes the Part file GFOpenUpTool INSTALL reads (gfopenup src/openuptool.c)
# for the part drawer the build made: C/wget2 to C:wget2, Docs/Wget2/ to
# SYS:Docs/Wget2/, and Libs/regex.library to LIBS: with "add" (where there
# is none or GFOpenUp put it, so an upgrade reaches it and another
# program's copy stays). HTTPS needs gfopentls.library (the GFOpenTLS part).
#   write-part.sh <part drawer> <version>
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
P=$1; V=$2
{
    echo "part wget2 $V"
    echo "; GNU Wget2: downloads over HTTP and HTTPS, whole sites too (C:wget2); HTTPS through gfopentls.library"
    echo "needs 68040"
    (cd "$P" && find C Docs -type f | LC_ALL=C sort) | while read -r f; do
        echo "copy \"$f\" \"SYS:$f\""
    done
    (cd "$P" && find Libs -type f | LC_ALL=C sort) | while read -r f; do
        echo "add \"$f\" \"LIBS:${f#Libs/}\""
    done
} > "$P/Part"
