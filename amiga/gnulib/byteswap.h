/* gnulib's byteswap.h (lib/byteswap.in.h) with GCC's built-ins.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef GFWGET_BYTESWAP_H
#define GFWGET_BYTESWAP_H
#define bswap_16(x) __builtin_bswap16(x)
#define bswap_32(x) __builtin_bswap32(x)
#define bswap_64(x) __builtin_bswap64(x)
#endif
