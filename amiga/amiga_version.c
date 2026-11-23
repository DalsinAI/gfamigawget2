/* Wget2's $VER string and stack: the Shell's 4 KB is far too little for
 * its option parsing and a TLS handshake (GFOpenTLS asks 16 KB of its
 * caller), so libnix's start-up gives it 128 KB.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include <config.h>
#ifndef GFWGET_DATE
#define GFWGET_DATE "16.11.2026"
#endif
const char gfwget_ver[] __attribute__((used)) = "$VER: wget2 " PACKAGE_VERSION " (" GFWGET_DATE ") GFOS";
unsigned long __stack = 131072;
