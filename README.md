# GFOpenWget

GFOS can now fetch files, and whole sites, straight from the Shell.
GFOpenWget is GNU Wget2 2.3.0 built for GFOS (AmigaOS 3.2 with GFOpenUp) on
a 68040 or 68060. It is installed as a GFOpenUp part, as `C:wget2`.

    wget2 https://example.org/file.lha            ; one file into the current drawer
    wget2 -c https://example.org/big.iso          ; carry on where an earlier try stopped
    wget2 -r -l 2 -np https://example.org/docs/   ; a site, two links deep

HTTPS goes through GFOpenTLS (`gfopentls.library`). Without it HTTP still
works, and HTTPS says what is missing. `--accept-regex` and
`--reject-regex` go through GFOpenRegex (`regex.library`, which comes with
the part). Any `bsdsocket.library` stack will do.

Install it with `GFOpenUpTool INSTALL Parts/wget2`.

Build it with the kitchen's open9 stove (stove_build, target `.`), or
`STOVE=.../os32-gcc16-open9 OUT=out sh build-amiga.sh`. How the port is
made, what it leaves out and how it is tested are in
[docs/PORT.md](docs/PORT.md).

GNU Wget2 is GPL-3.0-or-later and libwget is LGPL-3.0-or-later. GFOpenRegex
is MIT, and the port's own files are MIT.
