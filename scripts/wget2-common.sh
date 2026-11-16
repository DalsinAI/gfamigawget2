# What build-amiga.sh (the stove) and tests/host-wget2.sh (the PC) share:
# fetching Wget2, configure's answers (config.h) with its replacement
# names taken out, and the file lists. Sourced; needs HERE (the repo) and
# wget2.lock's settings.
#
# Wget2's own build links gnulib's lib/ (libgnu.a), whose header wrappers
# do not go with libnix's headers. Here configure only writes config.h,
# wgetver.h and (when the release has none) css_tokenizer.c; the few gnulib
# files Wget2 calls into are compiled as they are (WGET2_GNULIB), and what
# libnix lacks is ours (amiga/gfwget_compat.c, amiga/gfinet.c).
# MIT, Copyright (c) 2026 AmiCradle Team.

# libwget (libwget/Makefile.am libwget_la_SOURCES, no HTTP/2), with
# ssl_gfopentls.c (amiga/) in ssl_none.c's place
WGET2_LIBWGET="atom_url bar bitmap buffer buffer_printf base64 console cookie
 cookie_parse css css_url decompressor dns_cache encoding hash_printf hashfile
 hashmap io hsts hpkp hpkp_db html_url http http_parse init ip iri list log
 logger mem metalink net netrc ocsp pipe plugin printf random robots rss_url
 sitemap_url stringmap strlcpy strscpy thread tls_session utils vector xalloc
 xml http_highlevel error dns"
# the wget2 program (src/Makefile.am wget2_SOURCES, no GPGME)
WGET2_PROGRAM="bar blacklist dl host job log options plugin stats_server
 stats_site testing utils wget"
# the gnulib files of the release that Wget2 calls (lib/)
WGET2_GNULIB="xstrtol xstrtoul xstrtoll xstrtoull gettime safe-read safe-write
 basename-lgpl c-strcasecmp c-strncasecmp md2 md5 sha1 sha256 sha512"
# the gnulib headers those and Wget2's files include
WGET2_GNULIB_H="gettext.h c-ctype.h xstrtol.h timespec.h filename.h dirname.h
 c-strcase.h c-strcasestr.h md2.h md5.h sha1.h sha256.h sha512.h
 xalloc-oversized.h intprops.h intprops-internal.h attribute.h idx.h
 basename-lgpl.h safe-read.h safe-write.h stat-time.h verify.h minmax.h
 xgethostname.h arg-nonnull.h u64.h getpass.h glthread/thread.h glthread/lock.h
 glthread/cond.h"
# configure: what this build has no library for, or does not need
WGET2_CONFIGURE="--disable-threads --disable-nls --disable-doc --disable-shared
 --enable-static --disable-xattr --disable-manylibs --without-libpsl --without-libhsts
 --without-libnghttp2 --without-gpgme --without-zlib --without-brotlidec
 --without-zstd --without-lzip --without-bzip2 --without-lzma --without-libidn2
 --without-libidn --without-libpcre2 --without-libpcre --without-libmicrohttpd
 --without-plugin-support --without-libdane --with-ssl=none"

# Wget2 at WGET2_VERSION with patches/ applied, into $1 (redone when either
# changes). The release tarball, checked against WGET2_SHA256; WGET2_SRC=...
# takes a tree already on disk (a release unpacked, or a bootstrapped git
# checkout of tag v$WGET2_VERSION) where the tarball cannot be fetched.
wget2_source() {
    PSUM=$(cat "$HERE"/patches/*.patch | sha256sum | cut -c1-16)
    want="$WGET2_VERSION-${WGET2_SRC:+local-}$PSUM"
    if [ "$(cat "$1/.patched" 2>/dev/null)" != "$want" ]; then
        rm -rf "$1"; mkdir -p "$1"
        if [ -n "${WGET2_SRC:-}" ]; then
            (cd "$WGET2_SRC" && tar cf - --exclude=.git .) | (cd "$1" && tar xf -)
        else
            tgz="$(dirname "$1")/wget2-$WGET2_VERSION.tar.gz"
            if [ "$(sha256sum "$tgz" 2>/dev/null | cut -d' ' -f1)" != "$WGET2_SHA256" ]; then
                curl -fsSL -o "$tgz" "$WGET2_URL" || wget -q -O "$tgz" "$WGET2_URL"
            fi
            [ "$(sha256sum "$tgz" | cut -d' ' -f1)" = "$WGET2_SHA256" ] || { echo "wget2 tarball: sha256 mismatch"; exit 1; }
            tar xzf "$tgz" -C "$1" --strip-components=1
        fi
        [ -x "$1/configure" ] || { echo "$1: no configure (bootstrap the git checkout first)"; exit 1; }
        for p in "$HERE"/patches/*.patch; do
            patch -s -d "$1" -p1 < "$p" || { echo "patch failed: $p"; exit 1; }
        done
        echo "$want" > "$1/.patched"
    fi
}

# configure in $2 for the source $1 (more arguments go to configure), then
# config.h's "#define name rpl_name" lines out (no gnulib replacements are
# linked: the C library's, or ours, are the ones). GFOS=1 (the Amiga):
# config.h ends by including amiga/gfwget_compat.h; on the PC,
# amiga/gfwget_pc.h.
wget2_config() {
    src=$1; bld=$2; shift 2
    stamp="$(cat "$src/.patched")|$*|${GFOS:-0}"
    if [ "$(cat "$bld/.configured" 2>/dev/null)" != "$stamp" ]; then
        rm -rf "$bld"; mkdir -p "$bld"
        (cd "$bld" && "$src/configure" -q "$@" > configure.log 2>&1) || { tail -30 "$bld/config.log"; exit 1; }
        echo "$stamp" > "$bld/.configured"
    fi
    mkdir -p "$bld/gf" "$bld/gli/glthread"
    sed -e '/^#define [A-Za-z_][A-Za-z0-9_]* rpl_/d' "$bld/config.h" > "$bld/gf/config.h"
    # the plain gnulib headers Wget2's files include (not lib/'s wrappers
    # of the C library's own headers, nor its regex.h)
    for h in $WGET2_GNULIB_H; do cp "$src/lib/$h" "$bld/gli/$h"; done
    if [ "${GFOS:-0}" = 1 ]; then
        cat >> "$bld/gf/config.h" <<'EOF'

/* GFOS: what libnix lacks, our own (amiga/gfwget_compat.h) */
#define GFWGET_AMIGA 1
#include "gfwget_compat.h"
EOF
    else
        printf '\n/* the PC tests: amiga/gfwget_pc.h */\n#include "gfwget_pc.h"\n' >> "$bld/gf/config.h"
    fi
    # the CSS tokenizer: the release has it made; a git checkout needs flex
    if [ ! -f "$src/libwget/css_tokenizer.c" ] && [ ! -f "$bld/libwget/css_tokenizer.c" ]; then
        mkdir -p "$bld/libwget"
        flex -w -o "$bld/libwget/css_tokenizer.c" "$src/libwget/css_tokenizer.lex"
    fi
}

# the CSS tokenizer source for build $2 of source $1
wget2_css_tokenizer() {
    if [ -f "$1/libwget/css_tokenizer.c" ]; then echo "$1/libwget/css_tokenizer.c"; else echo "$2/libwget/css_tokenizer.c"; fi
}

# a Makefile in $1 compiling every Wget2 file with $CC $CF (gf/config.h from
# build $3 of source $2), objects listed as OBJS; WGET2_OURS: amiga/ files too
wget2_makefile() {
    o=$1; src=$2; bld=$3
    inc="-I$bld/gf -I$bld/include/wget -I$src/include/wget -I$HERE/amiga/gnulib -I$bld/gli -I$HERE/amiga"
    lw="-DBUILDING_LIBWGET -I$src/libwget -DWGETVER_FILE=\\\"wgetver.h\\\""
    mkdir -p "$o"
    {
        echo "CC = $CC"
        echo "CF = $CF $inc"
        A=""
        for f in $WGET2_LIBWGET; do
            A="$A lw_$f.o"
            printf 'lw_%s.o: %s %s\n\t$(CC) $(CF) %s -MMD -c $< -o $@\n' "$f" "$src/libwget/$f.c" "$bld/gf/config.h" "$lw"
        done
        A="$A lw_css_tokenizer.o"
        printf 'lw_css_tokenizer.o: %s %s\n\t$(CC) $(CF) %s -I%s -Wno-unused-function -c $< -o $@\n' \
            "$(wget2_css_tokenizer "$src" "$bld")" "$bld/gf/config.h" "$lw" "$src/libwget"
        A="$A lw_ssl_gfopentls.o"
        printf 'lw_ssl_gfopentls.o: %s %s\n\t$(CC) $(CF) %s $(TLS_CF) -MMD -c $< -o $@\n' \
            "$HERE/amiga/ssl_gfopentls.c" "$bld/gf/config.h" "$lw"
        for f in $WGET2_PROGRAM; do
            A="$A w_$f.o"
            printf 'w_%s.o: %s %s\n\t$(CC) $(CF) -I%s -I%s -I%s -MMD -c $< -o $@\n' "$f" "$src/src/$f.c" "$bld/gf/config.h" "$bld/src" "$src/src" "$src/include"
        done
        for f in $WGET2_GNULIB; do
            A="$A gl_$f.o"
            printf 'gl_%s.o: %s %s\n\t$(CC) $(CF) -I%s -MMD -c $< -o $@\n' "$f" "$src/lib/$f.c" "$bld/gf/config.h" "$src/lib"
        done
        for f in ${WGET2_OURS:-}; do
            A="$A our_$f.o"
            printf 'our_%s.o: %s %s\n\t$(CC) $(CF) -MMD -c $< -o $@\n' "$f" "$HERE/amiga/$f.c" "$bld/gf/config.h"
        done
        echo "OBJS =$A"
        echo "all: \$(OBJS)"
        echo "-include *.d"
    } > "$o/Makefile"
}

# a repository at one commit, fetched once: fetch_at DIR URL COMMIT
fetch_at() {
    if [ "$(git -C "$1" rev-parse HEAD 2>/dev/null)" != "$3" ]; then
        rm -rf "${1:?}"; mkdir -p "$1"
        git -C "$1" init -q
        git -C "$1" fetch -q --depth 1 "$2" "$3"
        git -C "$1" checkout -q FETCH_HEAD
        [ "$(git -C "$1" rev-parse HEAD)" = "$3" ] || { echo "$1: commit mismatch"; exit 1; }
    fi
}
