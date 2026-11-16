/* The PC tests' build (tests/host-wget2.sh): the two things gnulib's
 * header wrappers add to glibc that Wget2's files use. config.h includes
 * this at its end there, as it includes gfwget_compat.h on GFOS.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef GFWGET_PC_H
#define GFWGET_PC_H
#include <string.h>
#include <fcntl.h>
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef streq
static inline int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }
#endif
/* ours (amiga/gfwget_compat.c): gnulib's, without its xalloc */
char *base_name(const char *name);
char *xgethostname(void);
#endif
