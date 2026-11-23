/* C23's <stdckdint.h> (gnulib's lib/stdckdint.in.h) on GCC's built-ins,
 * for a compiler that has none (the PC tests' older GCC).
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef GFWGET_STDCKDINT_H
#define GFWGET_STDCKDINT_H
#define ckd_add(r, a, b) __builtin_add_overflow(a, b, r)
#define ckd_sub(r, a, b) __builtin_sub_overflow(a, b, r)
#define ckd_mul(r, a, b) __builtin_mul_overflow(a, b, r)
#endif
