/* What Wget2 takes from a POSIX C library and libnix lacks, our own
 * (amiga/gfwget_compat.c). config.h includes this at its end on the GFOS
 * build (scripts/wget2-common.sh wget2_config), so every Wget2 file sees it
 * first. Each call is renamed to gfwget_* with a function-like macro, so a
 * libnix header that declares the same name (glob, posix_spawnp) keeps its
 * own declaration and struct fields of the same name are left alone.
 *   futimens: AmigaDOS sets a written file's date when it is closed, so the
 *     date is kept and set by name after close (gfwget_closed()).
 *   glob: the wildcard in the last part of the path, with fnmatch().
 *   posix_spawn*: ENOSYS (no --use-askpass helper programs).
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#ifndef GFWGET_COMPAT_H
#define GFWGET_COMPAT_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <wchar.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <glob.h>
#ifndef GFWGET_HOST_TEST
#include <spawn.h>
#include <sys/file.h>
#include <sys/ioctl.h>   /* FIONBIO for libnix's sockets (patches/0001) */
/* libnix has it; its <time.h> declares it only with the POSIX timers */
int nanosleep(const struct timespec *req, struct timespec *rem);
#include "gfinet.h"
#endif

/* Wget2's folder (settings, HSTS and session files) when there is no
   $HOME; WGET2DIR (an environment variable) names another */
#define GFWGET_HOME "ENVARC:Wget2"
#define GFWGET_APPDATA "WGET2DIR"

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef GLOB_ONLYDIR
#define GLOB_ONLYDIR 0
#endif
#ifndef GLOB_TILDE_CHECK
#define GLOB_TILDE_CHECK GLOB_TILDE
#endif
#ifndef GLOB_NOMATCH
#define GLOB_NOMATCH (-3)
#endif
#ifndef LOCK_SH
#define LOCK_SH 1
#define LOCK_EX 2
#define LOCK_NB 4
#define LOCK_UN 8
#endif
#ifndef TIOCGWINSZ
/* the console answers no window size; Wget2 then takes 70 columns */
struct winsize { unsigned short ws_row, ws_col, ws_xpixel, ws_ypixel; };
#define TIOCGWINSZ 0x40087468
#endif

#ifndef GFWGET_HOST_TEST
#ifndef streq
static inline int streq(const char *a, const char *b) { return strcmp(a, b) == 0; }
#endif
#define random_data gfwget_random_data
#define strchrnul(s, c) gfwget_strchrnul(s, c)
#define random_r(d, r) gfwget_random_r(d, r)
#define initstate_r(seed, b, n, d) gfwget_initstate_r(seed, b, n, d)
#define srandom_r(seed, d) gfwget_srandom_r(seed, d)
#define qsort_r(b, n, sz, cmp, arg) gfwget_qsort_r(b, n, sz, cmp, arg)
#define sigaction(s, a, o) gfwget_sigaction(s, a, o)
#define setsid() gfwget_setsid()
#define getpass(p) gfwget_getpass(p)
#define futimens(fd, ts) gfwget_futimens(fd, ts)
#define flock(fd, op) gfwget_flock(fd, op)
#define canonicalize_file_name(p) gfwget_canonicalize_file_name(p)
#define wcwidth(c) gfwget_wcwidth(c)
#define glob(p, f, e, g) gfwget_glob(p, f, e, g)
#define globfree(g) gfwget_globfree(g)
#define posix_spawn_file_actions_init(a) gfwget_spawn_actions(a)
#define posix_spawn_file_actions_destroy(a) gfwget_spawn_actions(a)
#define posix_spawn_file_actions_addclose(a, fd) gfwget_spawn_actions(a)
#define posix_spawn_file_actions_adddup2(a, fd, nfd) gfwget_spawn_actions(a)
#define posix_spawnp(pid, f, a, at, argv, env) gfwget_spawnp(pid, f)
int gfwget_spawn_actions(void *actions);
int gfwget_spawnp(pid_t *pid, const char *file);
int gfwget_sigaction(int sig, const struct sigaction *act, struct sigaction *old);
pid_t gfwget_setsid(void);
#endif

/* random_r's state (glibc's has more; Wget2 only passes it back) */
struct gfwget_random_data { uint32_t s[4]; };
char *gfwget_strchrnul(const char *s, int c);
int gfwget_random_r(struct gfwget_random_data *d, int32_t *result);
int gfwget_initstate_r(unsigned int seed, char *buf, size_t n, struct gfwget_random_data *d);
int gfwget_srandom_r(unsigned int seed, struct gfwget_random_data *d);
void gfwget_qsort_r(void *base, size_t n, size_t size,
                    int (*cmp)(const void *, const void *, void *), void *arg);
char *gfwget_getpass(const char *prompt);
int gfwget_futimens(int fd, const struct timespec ts[2]);
/* after close(fd): the date futimens() asked for, set by name */
int gfwget_closed(int fd);
int gfwget_flock(int fd, int op);
char *gfwget_canonicalize_file_name(const char *path);
int gfwget_wcwidth(wchar_t c);
int gfwget_glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *g);
void gfwget_globfree(glob_t *g);
/* gnulib's (dirname.h, xgethostname.h), ours */
char *base_name(const char *name);
char *xgethostname(void);
#endif
