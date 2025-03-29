/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDLIB_H
#define STDLIB_H

#include <crt_impl/common.h>
#include <limits.h>
#include <stddef.h>
#include <threads.h>

#define __STDC_VERSION_STDLIB_H__ 202311L

#define RAND_MAX INT_MAX
#define MB_CUR_MAX (__mb_cur_max())

/* This should probably be OS-specific... */
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

typedef struct {
    long long quot;
    long long rem;
} lldiv_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Numeric conversion functions. */
double atof(const char *nptr);
int atoi(const char *nptr);
long atol(const char *nptr);
long long atoll(const char *nptr);
int strfromd(char *CRT_RESTRICT str, size_t n, const char **CRT_RESTRICT format, double fp);
int strfromf(char *CRT_RESTRICT str, size_t n, const char **CRT_RESTRICT format, float fp);
int strfroml(char *CRT_RESTRICT str, size_t n, const char **CRT_RESTRICT format, long double fp);
long double strtold(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr);
float strtof(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr);
double strtod(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr);
long strtol(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr, int base);
long long strtoll(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr, int base);
unsigned long strtoul(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr, int base);
unsigned long long strtoull(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr, int base);

/* Pseudo-random sequence generation functions. */
int rand(void);
void srand(unsigned seed);

/* Memory management functions. */
CRT_MALLOC CRT_ALLOC_ALIGN(1) CRT_ALLOC_SIZE(2) void *aligned_alloc(size_t alignment, size_t size);
CRT_MALLOC CRT_ALLOC_SIZE(1, 2) void *calloc(size_t nmemb, size_t size);
void free(void *ptr);
void free_sized(void *ptr, size_t size);
void free_aligned_sized(void *ptr, size_t alignment, size_t size);
CRT_MALLOC CRT_ALLOC_SIZE(1) void *malloc(size_t size);
CRT_ALLOC_SIZE(2) void *realloc(void *ptr, size_t size);

/* Communication with the environment. */
CRT_NORETURN void abort(void);
int atexit(void (*func)(void));
int at_quick_exit(void (*func)(void));
CRT_NORETURN void exit(int status);
CRT_NORETURN void _Exit(int status);
char *getenv(const char *name);
CRT_NORETURN void quick_exit(int status);
int system(const char *string);

/* Searching and sorting utilities. */
void *bsearch(
    const void *key,
    const void *base,
    size_t nmemb,
    size_t size,
    int (*compar)(const void *, const void *));
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));

/* Integer arithmetic functions. */
int abs(int j);
long int labs(long int j);
long long int llabs(long long int j);
div_t div(int numer, int denom);
ldiv_t ldiv(long int numer, long int denom);
lldiv_t lldiv(long long int numer, long long int denom);

/* Multibyte/wide character conversion functions. */
int mblen(const char *s, size_t n);
int mbtowc(wchar_t *CRT_RESTRICT pwc, const char *CRT_RESTRICT s, size_t n);
int wctomb(char *s, wchar_t wc);

/* Multibyte/wide string conversion functions. */
size_t mbstowcs(wchar_t *CRT_RESTRICT pwcs, const char *CRT_RESTRICT s, size_t n);
size_t wcstombs(char *CRT_RESTRICT s, const wchar_t *CRT_RESTRICT pwcs, size_t n);

/* Alignment of memory. */
size_t memalignment(const void *p);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STDLIB_H */
