# GFOpenWget: how the port is made

GNU Wget2 2.3.0, the downloader, for GFOS (AmigaOS 3.2.3 with GFOpenUp) on
a 68040 or 68060, installed as a GFOpenUp part: `C:wget2`. Files and whole
sites over HTTP and HTTPS, with HTTPS through GFOpenTLS (`gfopentls.library`)
and `--accept-regex`/`--reject-regex` through GFOpenRegex (`regex.library`).

    wget2 https://example.org/file.lha            ; one file into the current drawer
    wget2 -c https://example.org/big.iso          ; carry on where an earlier try stopped
    wget2 -N -O RAM:index.html https://example.org/
    wget2 -r -l 2 -np https://example.org/docs/   ; a site, two links deep
    wget2 -r --accept-regex '.*\.lha$' https://example.org/files/

Status: built for the PC from the same sources, settings and TLS code and
passing real downloads from local HTTP and HTTPS servers (see Tests); the
Amiga build is made by the stove (C:wget2 is about 500 KB), not yet run in
a lab.

## What you need

- GFOS: AmigaOS 3.2 with GFOpenUp, a 68040 or 68060. Soft float: the FPU
  is used when there is one (fpuauto), so an LC040 runs it too.
- A TCP/IP stack: `bsdsocket.library` (GFOpenSocket on GFOS, or any other).
- For HTTPS: `gfopentls.library` 1.5 or later (the GFOpenTLS part), with
  its trust store (`S:GFOpenTLS/ca-bundle.pem`, `ENVARC:GFOpenTLS/certs/`).
  Without it, HTTP works and HTTPS says what is missing.
- `regex.library` comes with the part (added to LIBS: where there is none,
  or where GFOpenUp put it).

Install with `GFOpenUpTool INSTALL Parts/wget2`. The manual is
`SYS:Docs/Wget2/wget2.md` (the same as `wget2 --help` in full).

## On AmigaOS

- Settings: `ENVARC:Wget2/wget2rc` for everyone; Wget2's own files (HSTS,
  TLS sessions, the HPKP and OCSP lists) go in `ENVARC:Wget2`, or in the
  folder the environment variable `WGET2DIR` names, or `$HOME` when that is
  set.
- HTTPS: `--ca-certificate=FILE` adds a PEM file to GFOpenTLS's trust
  store; `--ca-directory=DIR` makes the PEM files of DIR the whole store.
  `--no-check-certificate` goes on with a certificate that does not check
  out (and says why); `--check-hostname=off` takes any host name.
  `--secure-protocol` takes `TLSv1_2`, `TLSv1_3` or `auto`. A second
  connection to the same host in one run resumes the first one's session.
- Not offered: HTTP/2 (no nghttp2), IPv6 (an IPv6 address says "Address
  family not supported"), client certificates, OCSP and CRL checks, HPKP
  pins, compressed transfers (`--compression`), internationalised domain
  names, the progress bar (it needs threads: Wget2 prints a line per file
  instead), `-b` (background: use `Run`), `--use-askpass`, extended
  attributes, plugins, Metalink signatures (GPGME). Downloads go one at a
  time (`--max-threads` has no threads to use).
- `--ask-password` reads the password without showing it.
- `-N` dates: AmigaDOS dates a written file when it is closed, so the
  server's date is put on the file just after (`patches/0002`).
- 128 KB of stack is set in the program, so the Shell's 4 KB is enough.

## Building

    STOVE=~/AmiCradle/stoves/os32-gcc16-open9 OUT=out sh build-amiga.sh

or in Cradle, stove_build with the os32-gcc16-open9 stove, target `.`.
The Wget2 release tarball (checked against its SHA-256), GFOpenRegex,
GFOpenTLS's headers and fpuauto are fetched as `wget2.lock` gives.
`WGET2_SRC=...` takes a Wget2 tree already on disk (the release unpacked,
or a git checkout of tag v2.3.0 after `./bootstrap`). `OUT` gets
`Parts/wget2` (the part). A stove without its own NDK (the bare gfopengcc
release) takes `NDK_INCLUDE=.../gfopensdk/OpenNDK/Include_H`.

How it is built: Wget2's configure runs only to write `config.h`,
`wgetver.h` and the CSS tokenizer. Its own build would link gnulib's
`lib/` (libgnu.a), whose wrappers of the C library's headers do not go
with libnix's, so `scripts/wget2-common.sh` compiles libwget, the program
and the few gnulib files Wget2 calls (hashes, `xstrtol`, `safe_read`...)
itself, with `config.h`'s replacement names (`#define x rpl_x`) taken out
and our own calls for what libnix lacks. Sockets are libnix's socket layer
(`-lsocket`): a socket is a file descriptor, so Wget2's `poll`, `connect`,
`send` and `recv` work as upstream wrote them.

## Tests

`make -C tests test` on a PC: `tests/host-wget2.sh` builds Wget2 for the
PC from the same patched sources, file lists and TLS backend (over
GFOpenTLS's host build at the commit `wget2.lock` pins), and runs:

- `tests/compat_test.c`: our `strchrnul`, `random_r`, `qsort_r`,
  `wcwidth`, `glob`, the date kept until close, `canonicalize_file_name`,
  `base_name`, `xgethostname` and `flock`, each against glibc's;
- `tests/gfinet_test.c`: `inet_pton`, `inet_ntop`, `getaddrinfo` and
  `getnameinfo` against the C library's own;
- `tests/test-part.sh`: the Part file;
- real downloads from local servers (`tests/serve.py`, a test site from
  `tests/make_site.py`, a test CA made with openssl): a page; a site with
  `-r` (links in HTML and CSS); `-N` putting the server's date on the
  file; `--accept-regex`; 1 MB over HTTPS through GFOpenTLS; a certificate
  from another CA refused; a wrong host name refused;
  `--no-check-certificate`; a second HTTPS connection resuming the first
  one's TLS 1.3 session.

What only a lab can show: the bsdsocket stack under load, non-blocking
sockets through libnix's `FIONBIO`, `poll` on libnix's socket descriptors,
Ctrl-C during a download, GFOpenTLS's speed on a real 68040, and the dates
AmigaDOS keeps.

## What was changed

- `patches/0001`: sockets are made non-blocking with `ioctl(FIONBIO)`
  (libnix passes it to `IoctlSocket`) in place of `fcntl(O_NONBLOCK)`.
- `patches/0002`: after a download is closed, the server's date goes on
  the file (`gfwget_closed()`).
- `patches/0003`: Wget2's folder is `ENVARC:Wget2` (or `WGET2DIR`, or
  `$HOME`), not `./.config/wget` and `./.local/share/wget`.
- `amiga/ssl_gfopentls.c`: libwget's TLS calls (`wget_ssl_*`) over
  GFOpenTLS, in `ssl_none.c`'s place: an I/O hook over Wget2's own socket,
  Wget2's timeouts, SNI, verification modes, ALPN, sessions per host.
- `amiga/gfwget_compat.c`: `strchrnul`, `random_r`, `qsort_r`, `glob`,
  `wcwidth`, `getpass`, `futimens`, `flock`, `canonicalize_file_name`,
  `sigaction`, `setsid` and `posix_spawn` saying no, gnulib's `base_name`
  and `xgethostname`.
- `amiga/gfinet.c`: `getaddrinfo`, `getnameinfo`, `inet_pton`, `inet_ntop`
  of our own, compiled with the same headers as Wget2.
- `amiga/gnulib/`: `byteswap.h`, `sockets.h` and `stdckdint.h` for the
  gnulib files compiled as they are.

GNU Wget2 is GPL-3.0-or-later and libwget LGPL-3.0-or-later (Free Software
Foundation, `SYS:Docs/Wget2/COPYING-Wget2`); GFOpenRegex is MIT
(`SYS:Docs/Wget2/LICENSE-GFOpenRegex`); the port's own files are MIT.

Designed, tested and delivered by The AmiCradle Team.
