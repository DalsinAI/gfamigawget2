/* What Wget2 takes from a POSIX C library and libnix lacks (see
 * gfwget_compat.h). The PC tests (tests/compat_test.c) build this file with
 * GFWGET_HOST_TEST and hold each call to glibc's.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fnmatch.h>
#include <unistd.h>
#include "gfwget_compat.h"
#ifdef __amigaos__
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <utime.h>
/* libnix: the AmigaDOS file handle behind a descriptor */
extern BPTR fdtofh(int fd);
#else
#include <utime.h>
#include <sys/time.h>
#endif

char *gfwget_strchrnul(const char *s, int c)
{
    while (*s && *s != (char)c)
        s++;
    return (char *)s;
}

/* ---- random_r: xoshiro128** ----------------------------------------------- */
static uint32_t rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

int gfwget_srandom_r(unsigned int seed, struct gfwget_random_data *d)
{
    uint32_t z = seed;
    int i;
    if (!d) { errno = EINVAL; return -1; }
    for (i = 0; i < 4; i++) {   /* splitmix32 spreads the seed over the state */
        z += 0x9e3779b9u;
        uint32_t t = z;
        t = (t ^ (t >> 16)) * 0x85ebca6bu;
        t = (t ^ (t >> 13)) * 0xc2b2ae35u;
        d->s[i] = t ^ (t >> 16);
    }
    if (!(d->s[0] | d->s[1] | d->s[2] | d->s[3]))
        d->s[0] = 1;
    return 0;
}

int gfwget_initstate_r(unsigned int seed, char *buf, size_t n, struct gfwget_random_data *d)
{
    if (!buf || n < 8) { errno = EINVAL; return -1; }
    return gfwget_srandom_r(seed, d);
}

int gfwget_random_r(struct gfwget_random_data *d, int32_t *result)
{
    uint32_t *s, r, t;
    if (!d || !result) { errno = EINVAL; return -1; }
    s = d->s;
    r = rotl(s[1] * 5, 7) * 9;
    t = s[1] << 9;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t; s[3] = rotl(s[3], 11);
    *result = (int32_t)(r >> 1);   /* 0 .. 2^31-1, as random() */
    return 0;
}

/* ---- qsort_r (glibc's argument order): heapsort ------------------------------ */
static void swap_bytes(unsigned char *a, unsigned char *b, size_t n)
{
    while (n--) { unsigned char t = *a; *a++ = *b; *b++ = t; }
}

static void sift(unsigned char *b, size_t root, size_t n, size_t sz,
                 int (*cmp)(const void *, const void *, void *), void *arg)
{
    for (;;) {
        size_t child = 2 * root + 1, big = root;
        if (child < n && cmp(b + big * sz, b + child * sz, arg) < 0)
            big = child;
        if (child + 1 < n && cmp(b + big * sz, b + (child + 1) * sz, arg) < 0)
            big = child + 1;
        if (big == root)
            return;
        swap_bytes(b + root * sz, b + big * sz, sz);
        root = big;
    }
}

void gfwget_qsort_r(void *base, size_t n, size_t size,
                    int (*cmp)(const void *, const void *, void *), void *arg)
{
    unsigned char *b = base;
    size_t i;
    if (n < 2 || !size)
        return;
    for (i = n / 2; i-- > 0;)
        sift(b, i, n, size, cmp, arg);
    for (i = n - 1; i > 0; i--) {
        swap_bytes(b, b + i * size, size);
        sift(b, 0, i, size, cmp, arg);
    }
}

/* ---- file dates: set by name once the file is closed ------------------------- */
#define GFW_DATES 8
static struct { int fd; time_t mtime; char *name; } dates[GFW_DATES];

static char *fd_name(int fd)
{
#ifdef __amigaos__
    char buf[512];
    BPTR fh = fdtofh(fd);
    if (!fh || !NameFromFH(fh, (STRPTR)buf, sizeof buf))
        return NULL;
    return strdup(buf);
#else
    char link[64], buf[4096];
    ssize_t n;
    snprintf(link, sizeof link, "/proc/self/fd/%d", fd);
    n = readlink(link, buf, sizeof buf - 1);
    if (n <= 0)
        return NULL;
    buf[n] = 0;
    return strdup(buf);
#endif
}

int gfwget_futimens(int fd, const struct timespec ts[2])
{
    int i, free_slot = -1;
    time_t t;
    if (!ts) {
        t = time(NULL);
    } else {
        t = ts[1].tv_sec;
    }
    for (i = 0; i < GFW_DATES; i++) {
        if (dates[i].name && dates[i].fd == fd) { free_slot = i; break; }
        if (!dates[i].name && free_slot < 0) free_slot = i;
    }
    if (free_slot < 0) { errno = ENOMEM; return -1; }
    if (!dates[free_slot].name) {
        dates[free_slot].name = fd_name(fd);
        if (!dates[free_slot].name) { errno = EBADF; return -1; }
    }
    dates[free_slot].fd = fd;
    dates[free_slot].mtime = t;
    return 0;
}

int gfwget_closed(int fd)
{
    int i, rc = 0;
    for (i = 0; i < GFW_DATES; i++) {
        if (dates[i].name && dates[i].fd == fd) {
            struct utimbuf u;
            u.actime = dates[i].mtime;
            u.modtime = dates[i].mtime;
            if (utime(dates[i].name, &u) != 0)
                rc = -1;
            free(dates[i].name);
            dates[i].name = NULL;
        }
    }
    return rc;
}

int gfwget_flock(int fd, int op)
{
    /* one Wget2 at a time writes its HSTS and cookie files on GFOS */
    (void)op;
    if (fd < 0) { errno = EBADF; return -1; }
    return 0;
}

char *gfwget_canonicalize_file_name(const char *path)
{
#ifdef __amigaos__
    char buf[512];
    BPTR lock;
    if (!path) { errno = EINVAL; return NULL; }
    lock = Lock((CONST_STRPTR)path, SHARED_LOCK);
    if (!lock) { errno = ENOENT; return NULL; }
    if (!NameFromLock(lock, (STRPTR)buf, sizeof buf)) {
        UnLock(lock);
        errno = ENAMETOOLONG;
        return NULL;
    }
    UnLock(lock);
    return strdup(buf);
#else
    if (!path) { errno = EINVAL; return NULL; }
    return realpath(path, NULL);
#endif
}

char *gfwget_getpass(const char *prompt)
{
    static char pw[128];
    size_t n = 0;
    int c;
#ifdef __amigaos__
    BPTR in = Input();
    int raw = in && IsInteractive(in) && SetMode(in, 1);
#endif
    fputs(prompt ? prompt : "", stderr);
    fflush(stderr);
    for (;;) {
#ifdef __amigaos__
        unsigned char ch;
        if (raw) {
            if (Read(in, &ch, 1) != 1) { c = EOF; } else { c = ch; }
            if (c == '\r') c = '\n';
        } else
#endif
        c = getchar();
        if (c == EOF || c == '\n')
            break;
        if ((c == 8 || c == 127) && n > 0) { n--; continue; }
        if (n + 1 < sizeof pw)
            pw[n++] = (char)c;
    }
    pw[n] = 0;
#ifdef __amigaos__
    if (raw) SetMode(in, 0);
#endif
    fputc('\n', stderr);
    return pw;
}

/* ---- wcwidth: 0 for combining marks, 2 for East Asian wide ones -------------- */
int gfwget_wcwidth(wchar_t wc)
{
    static const struct { unsigned long lo, hi; } zero[] = {
        {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x0610, 0x061A},
        {0x064B, 0x065F}, {0x0E31, 0x0E31}, {0x0E34, 0x0E3A}, {0x1AB0, 0x1AFF},
        {0x1DC0, 0x1DFF}, {0x200B, 0x200F}, {0x20D0, 0x20FF}, {0xFE00, 0xFE0F},
        {0xFE20, 0xFE2F}
    }, wide[] = {
        {0x1100, 0x115F}, {0x2E80, 0x303E}, {0x3041, 0x33FF}, {0x3400, 0x4DBF},
        {0x4E00, 0x9FFF}, {0xA000, 0xA4CF}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF},
        {0xFE30, 0xFE4F}, {0xFF00, 0xFF60}, {0xFFE0, 0xFFE6}, {0x1F300, 0x1F64F},
        {0x1F900, 0x1F9FF}, {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD}
    };
    unsigned long c = (unsigned long)wc;
    size_t i;
    if (c == 0)
        return 0;
    if (c < 32 || (c >= 0x7F && c < 0xA0))
        return -1;
    for (i = 0; i < sizeof zero / sizeof zero[0]; i++)
        if (c >= zero[i].lo && c <= zero[i].hi)
            return 0;
    for (i = 0; i < sizeof wide / sizeof wide[0]; i++)
        if (c >= wide[i].lo && c <= wide[i].hi)
            return 2;
    return 1;
}

/* ---- glob: a wildcard in the last part of the path --------------------------- */
static int add_path(glob_t *g, const char *path)
{
    char **v = realloc(g->gl_pathv, (size_t)(g->gl_pathc + 2) * sizeof *v);
    if (!v)
        return GLOB_NOSPACE;
    g->gl_pathv = v;
    if (!(v[g->gl_pathc] = strdup(path)))
        return GLOB_NOSPACE;
    v[++g->gl_pathc] = NULL;
    return 0;
}

static int cmp_names(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

static int is_dir(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int gfwget_glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *g)
{
    char *pat = NULL, *dir, *base, *sep;
    const char *home;
    DIR *d;
    struct dirent *e;
    int rc = 0, magic;
    size_t dl;

    (void)errfunc;
    if (!pattern || !g)
        return GLOB_NOSPACE;
    memset(g, 0, sizeof *g);
    /* ~/ -> $HOME/ (GLOB_TILDE) */
    if ((flags & GLOB_TILDE) && pattern[0] == '~' && (pattern[1] == '/' || !pattern[1])
        && (home = getenv("HOME")) && *home) {
        size_t hl = strlen(home);
        pat = malloc(hl + strlen(pattern) + 2);
        if (!pat) return GLOB_NOSPACE;
        strcpy(pat, home);
        if (pattern[1] && (home[hl - 1] == '/' || home[hl - 1] == ':'))
            strcat(pat, pattern + 2);
        else
            strcat(pat, pattern + 1);
    } else if (!(pat = strdup(pattern))) {
        return GLOB_NOSPACE;
    }
    magic = strpbrk(pat, "*?[") != NULL;
    if (!magic) {
        struct stat st;
        if (stat(pat, &st) == 0 || (flags & GLOB_NOCHECK)) {
            if ((flags & GLOB_MARK) && is_dir(pat) && pat[strlen(pat) - 1] != '/') {
                char *m = malloc(strlen(pat) + 2);
                if (!m) { free(pat); return GLOB_NOSPACE; }
                strcpy(m, pat); strcat(m, "/");
                rc = add_path(g, m);
                free(m);
            } else {
                rc = add_path(g, pat);
            }
        } else {
            rc = GLOB_NOMATCH;
        }
        free(pat);
        return rc;
    }
    /* the folder part (up to the last '/' or ':') must have no wildcard */
    sep = strrchr(pat, '/');
    if (!sep) sep = strrchr(pat, ':');
    if (sep) {
        dl = (size_t)(sep - pat) + 1;
        base = sep + 1;
    } else {
        dl = 0;
        base = pat;
    }
    if (memchr(pat, '*', dl) || memchr(pat, '?', dl) || memchr(pat, '[', dl)) {
        free(pat);
        return GLOB_NOMATCH;   /* not needed by Wget2 */
    }
    dir = malloc(dl + 2);
    if (!dir) { free(pat); return GLOB_NOSPACE; }
    if (dl) {
        memcpy(dir, pat, dl);
        dir[dl] = 0;
    } else {
#ifdef __amigaos__
        dir[0] = 0;    /* the current folder is "" on AmigaDOS */
#else
        strcpy(dir, ".");
#endif
    }
    d = opendir(dir);
    if (d) {
        while ((e = readdir(d)) != NULL) {
            char *full;
            if (e->d_name[0] == '.' && base[0] != '.')
                continue;
            if (fnmatch(base, e->d_name, 0) != 0)
                continue;
            full = malloc(dl + strlen(e->d_name) + 2);
            if (!full) { rc = GLOB_NOSPACE; break; }
            memcpy(full, pat, dl);
            strcpy(full + dl, e->d_name);
            if ((flags & GLOB_MARK) && is_dir(full))
                strcat(full, "/");
            rc = add_path(g, full);
            free(full);
            if (rc) break;
        }
        closedir(d);
    }
    free(dir);
    if (!rc && !g->gl_pathc) {
        if (flags & GLOB_NOCHECK)
            rc = add_path(g, pattern);
        else
            rc = GLOB_NOMATCH;
    }
    if (!rc && g->gl_pathc > 1 && !(flags & GLOB_NOSORT))
        qsort(g->gl_pathv, (size_t)g->gl_pathc, sizeof(char *), cmp_names);
    free(pat);
    if (rc && rc != GLOB_NOMATCH)
        gfwget_globfree(g);
    return rc;
}

void gfwget_globfree(glob_t *g)
{
    int i;
    if (!g || !g->gl_pathv)
        return;
    for (i = 0; i < (int)g->gl_pathc; i++)
        free(g->gl_pathv[i]);
    free(g->gl_pathv);
    g->gl_pathv = NULL;
    g->gl_pathc = 0;
}

#ifndef GFWGET_HOST_TEST
/* ---- signals, sessions, helper programs -------------------------------------- */
int gfwget_sigaction(int sig, const struct sigaction *act, struct sigaction *old)
{
    void (*prev)(int);
    if (act) {
        prev = signal(sig, act->sa_handler);
        if (prev == SIG_ERR) { errno = EINVAL; return -1; }
    } else {
        prev = signal(sig, SIG_DFL);
        if (prev != SIG_ERR)
            signal(sig, prev);
    }
    if (old) {
        memset(old, 0, sizeof *old);
        old->sa_handler = prev == SIG_ERR ? SIG_DFL : prev;
    }
    return 0;
}

pid_t gfwget_setsid(void)
{
    errno = EPERM;
    return (pid_t)-1;
}

int gfwget_spawn_actions(void *actions)
{
    (void)actions;
    return 0;
}

int gfwget_spawnp(pid_t *pid, const char *file)
{
    (void)file;
    if (pid) *pid = 0;
    return ENOSYS;
}
#endif

/* ---- gnulib's base_name and xgethostname, without its xalloc ----------------- */
char *base_name(const char *name)
{
    const char *b = name, *p;
    size_t n;
    /* past the last '/' (or AmigaDOS ':'); trailing slashes stay, as gnulib's */
    for (p = name; *p; p++)
        if ((*p == '/' || *p == ':') && p[1] && p[1] != '/')
            b = p + 1;
    n = strlen(b);
    if (!n && *name)
        n = 1, b = name + strlen(name) - 1;   /* "/" stays "/" */
    {
        char *r = malloc(n + 1);
        if (!r) abort();
        memcpy(r, b, n);
        r[n] = 0;
        return r;
    }
}

char *xgethostname(void)
{
    char buf[256];
    if (gethostname(buf, sizeof buf - 1) != 0)
        return NULL;
    buf[sizeof buf - 1] = 0;
    return strdup(buf);
}
