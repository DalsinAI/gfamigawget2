/* PC unit test for amiga/gfwget_compat.c (built with GFWGET_HOST_TEST):
 * each call held to glibc's, or to what Wget2 needs of it.
 * MIT, Copyright (c) 2026 AmiCradle Team. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include "gfwget_compat.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int by_int(const void *a, const void *b, void *arg)
{
	int x = *(const int *)a, y = *(const int *)b;
	(*(int *)arg)++;
	return (x > y) - (x < y);
}

static int by_int_plain(const void *a, const void *b)
{
	int x = *(const int *)a, y = *(const int *)b;
	return (x > y) - (x < y);
}

struct rec { char name[13]; int key; };
static int by_key_desc(const void *a, const void *b, void *arg)
{
	(void)arg;
	return ((const struct rec *)b)->key - ((const struct rec *)a)->key;
}

static void touch(const char *p)
{
	FILE *f = fopen(p, "w");
	if (f) { fputs("x", f); fclose(f); }
}

static int same_glob(const char *pat, int flags)
{
	glob_t a, b;
	int ra = gfwget_glob(pat, flags, NULL, &a), rb = glob(pat, flags, NULL, &b), ok = 1;
	size_t i;
	if ((ra == 0) != (rb == 0)) {
		printf("FAIL glob \"%s\": ours %d, glibc %d\n", pat, ra, rb);
		ok = 0;
	} else if (ra == 0) {
		if ((size_t)a.gl_pathc != (size_t)b.gl_pathc) ok = 0;
		for (i = 0; ok && i < (size_t)a.gl_pathc; i++)
			if (strcmp(a.gl_pathv[i], b.gl_pathv[i])) ok = 0;
		if (!ok) {
			printf("FAIL glob \"%s\":", pat);
			for (i = 0; i < (size_t)a.gl_pathc; i++) printf(" %s", a.gl_pathv[i]);
			printf(" | glibc:");
			for (i = 0; i < (size_t)b.gl_pathc; i++) printf(" %s", b.gl_pathv[i]);
			printf("\n");
		}
	}
	if (ra == 0) gfwget_globfree(&a);
	if (rb == 0) globfree(&b);
	if (!ok) fails++;
	return ok;
}

int main(void)
{
	char buf[64];
	int i, n;

	/* strchrnul */
	{
		const char *s = "wget2/amiga";
		CHECK(gfwget_strchrnul(s, '/') == strchrnul(s, '/'));
		CHECK(gfwget_strchrnul(s, 'z') == strchrnul(s, 'z'));
		CHECK(gfwget_strchrnul(s, 0) == strchrnul(s, 0));
		CHECK(gfwget_strchrnul("", 'a') == (char *)"" || *gfwget_strchrnul("", 'a') == 0);
	}

	/* random_r: in range, the same seed the same numbers, others differ */
	{
		struct gfwget_random_data a, b;
		char st[64];
		int32_t x, y, first = 0;
		int differ = 0;
		CHECK(gfwget_initstate_r(42, st, sizeof st, &a) == 0);
		CHECK(gfwget_initstate_r(42, st, sizeof st, &b) == 0);
		for (i = 0; i < 10000; i++) {
			CHECK(gfwget_random_r(&a, &x) == 0 && gfwget_random_r(&b, &y) == 0);
			if (x != y || x < 0) { CHECK(x == y && x >= 0); break; }
			if (!i) first = x; else if (x != first) differ = 1;
		}
		CHECK(differ);
		gfwget_srandom_r(43, &b);
		gfwget_random_r(&b, &y);
		gfwget_srandom_r(42, &a);
		gfwget_random_r(&a, &x);
		CHECK(x == first && y != first);
		CHECK(gfwget_initstate_r(1, st, 4, &a) == -1 && errno == EINVAL);
		CHECK(gfwget_random_r(NULL, &x) == -1);
	}

	/* qsort_r: glibc's argument order, any size, the result sorted as qsort's */
	{
		static int v[1000], w[1000];
		struct rec r[7] = { {"a", 3}, {"b", 9}, {"c", 1}, {"d", 9}, {"e", 0}, {"f", 5}, {"g", 2} };
		int calls = 0;
		srand(7);
		for (n = 0; n <= 1000; n = n ? n * 3 + 1 : 1) {
			for (i = 0; i < n; i++) v[i] = w[i] = rand() % 50;
			gfwget_qsort_r(v, (size_t)n, sizeof v[0], by_int, &calls);
			qsort(w, (size_t)n, sizeof w[0], by_int_plain);
			CHECK(!memcmp(v, w, (size_t)n * sizeof v[0]));
		}
		CHECK(calls > 0);
		gfwget_qsort_r(r, 7, sizeof r[0], by_key_desc, NULL);
		for (i = 1; i < 7; i++) CHECK(r[i - 1].key >= r[i].key);
	}

	/* wcwidth: as glibc's in a UTF-8 locale, for the characters checked */
	if (setlocale(LC_CTYPE, "C.UTF-8") || setlocale(LC_CTYPE, "en_US.UTF-8")) {
		static const unsigned long cs[] = { 'A', ' ', '~', 0x7f, 0x1b, 0x0a, 0xa0, 0xe9, 0x301, 0x20ac,
			0x3042, 0x4e2d, 0xac00, 0xff21, 0x1f600, 0x200b, 0xfe0f, 0x20000, 0x0410, 0x05d0 };
		for (i = 0; i < (int)(sizeof cs / sizeof cs[0]); i++) {
			int a = gfwget_wcwidth((wchar_t)cs[i]), b = wcwidth((wchar_t)cs[i]);
			if (a != b) { printf("FAIL wcwidth U+%04lX: ours %d, glibc %d\n", cs[i], a, b); fails++; }
		}
		CHECK(gfwget_wcwidth(0) == 0);
	}

	/* glob: a wildcard in the last part, as glibc's */
	{
		char d[] = "gfglobXXXXXX";
		CHECK(mkdtemp(d) != NULL);
		snprintf(buf, sizeof buf, "%s/a.txt", d); touch(buf);
		snprintf(buf, sizeof buf, "%s/b.txt", d); touch(buf);
		snprintf(buf, sizeof buf, "%s/c.html", d); touch(buf);
		snprintf(buf, sizeof buf, "%s/.hidden.txt", d); touch(buf);
		snprintf(buf, sizeof buf, "%s/sub", d); mkdir(buf, 0755);
		snprintf(buf, sizeof buf, "%s/*.txt", d); same_glob(buf, 0);
		snprintf(buf, sizeof buf, "%s/?.html", d); same_glob(buf, 0);
		snprintf(buf, sizeof buf, "%s/[ab].txt", d); same_glob(buf, 0);
		snprintf(buf, sizeof buf, "%s/*", d); same_glob(buf, GLOB_MARK);
		snprintf(buf, sizeof buf, "%s/*.none", d); same_glob(buf, 0);
		snprintf(buf, sizeof buf, "%s/*.none", d); same_glob(buf, GLOB_NOCHECK);
		snprintf(buf, sizeof buf, "%s/a.txt", d); same_glob(buf, 0);
		snprintf(buf, sizeof buf, "%s/sub", d); same_glob(buf, GLOB_MARK);
		snprintf(buf, sizeof buf, "%s/missing", d); same_glob(buf, 0);
		snprintf(buf, sizeof buf, "%s/.h*", d); same_glob(buf, 0);
		same_glob("~", GLOB_TILDE);
		same_glob("~/", GLOB_TILDE);
		{
			glob_t g;
			setenv("HOME", d, 1);
			CHECK(gfwget_glob("~", GLOB_TILDE, NULL, &g) == 0 && g.gl_pathc == 1 && !strcmp(g.gl_pathv[0], d));
			gfwget_globfree(&g);
			snprintf(buf, sizeof buf, "%s/a.txt", d);
			CHECK(gfwget_glob("~/a.txt", GLOB_TILDE, NULL, &g) == 0 && !strcmp(g.gl_pathv[0], buf));
			gfwget_globfree(&g);
		}

		/* futimens: the date is kept and set by name after close */
		{
			struct timespec ts[2];
			struct stat st;
			int fd;
			snprintf(buf, sizeof buf, "%s/dated.bin", d);
			fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			CHECK(fd >= 0);
			CHECK(write(fd, "data", 4) == 4);
			ts[0].tv_sec = ts[1].tv_sec = 1704110400;   /* 1 Jan 2024 */
			ts[0].tv_nsec = ts[1].tv_nsec = 0;
			CHECK(gfwget_futimens(fd, ts) == 0);
			close(fd);
			CHECK(gfwget_closed(fd) == 0);
			CHECK(stat(buf, &st) == 0 && st.st_mtime == 1704110400);
			CHECK(gfwget_closed(fd) == 0);   /* nothing kept any more */
			CHECK(gfwget_futimens(-1, ts) == -1);
		}

		/* canonicalize_file_name as realpath; a missing file is NULL */
		{
			char *a, *b;
			snprintf(buf, sizeof buf, "%s/../%s/a.txt", d, d);
			a = gfwget_canonicalize_file_name(buf);
			b = realpath(buf, NULL);
			CHECK(a && b && !strcmp(a, b));
			free(a); free(b);
			snprintf(buf, sizeof buf, "%s/none", d);
			CHECK(gfwget_canonicalize_file_name(buf) == NULL);
		}
		snprintf(buf, sizeof buf, "rm -rf %s", d);
		CHECK(system(buf) == 0);
	}

	/* base_name as gnulib's: the last part, trailing slashes kept */
	{
		static const char *in[] = { "a/b/c.txt", "c.txt", "a/b/", "/", "dir//", "SYS:C/wget2", "Work:", NULL };
		static const char *out[] = { "c.txt", "c.txt", "b/", "/", "dir//", "wget2", "Work:" };
		for (i = 0; in[i]; i++) {
			char *b = base_name(in[i]);
			if (strcmp(b, out[i])) { printf("FAIL base_name(\"%s\") = \"%s\", want \"%s\"\n", in[i], b, out[i]); fails++; }
			free(b);
		}
	}

	/* xgethostname as gethostname */
	{
		char h[256], *x = xgethostname();
		CHECK(gethostname(h, sizeof h) == 0 && x && !strcmp(x, h));
		free(x);
	}

	CHECK(gfwget_flock(0, LOCK_EX) == 0 && gfwget_flock(-1, LOCK_EX) == -1);

	if (!fails) printf("compat: all checks passed\n");
	return fails != 0;
}
