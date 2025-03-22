/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDLIB_H
#define STDLIB_H

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
int strfromd(char *restrict str, size_t n, const char **restrict format, double fp);
int strfromf(char *restrict str, size_t n, const char **restrict format, float fp);
int strfroml(char *restrict str, size_t n, const char **restrict format, long double fp);
long double strtold(const char *restrict nptr, char **restrict endptr);
float strtof(const char *restrict nptr, char **restrict endptr);
double strtod(const char *restrict nptr, char **restrict endptr);
long strtol(const char *restrict nptr, char **restrict endptr, int base);
long long strtoll(const char *restrict nptr, char **restrict endptr, int base);
unsigned long strtoul(const char *restrict nptr, char **restrict endptr, int base);
unsigned long long strtoull(const char *restrict nptr, char **restrict endptr, int base);

/* Pseudo-random sequence generation functions. */
int rand(void);
void srand(unsigned seed);

/* Memory management functions. */
void *aligned_alloc(size_t alignment, size_t size);
void *calloc(size_t nmemb, size_t size);
void free(void *ptr);
void free_sized(void *ptr, size_t size);
void free_aligned_sized(void *ptr, size_t alignment, size_t size);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);

/* Communication with the environment. */
[[noreturn]] void abort(void);
int atexit(void (*func)(void));
int at_quick_exit(void (*func)(void));
[[noreturn]] void exit(int status);
[[noreturn]] void _Exit(int status);
char *getenv(const char *name);
[[noreturn]] void quick_exit(int status);
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
int mbtowc(wchar_t *restrict pwc, const char *restrict s, size_t n);
int wctomb(char *s, wchar_t wc);

/* Multibyte/wide string conversion functions. */
size_t mbstowcs(wchar_t *restrict pwcs, const char *restrict s, size_t n);
size_t wcstombs(char *restrict s, const wchar_t *restrict pwcs, size_t n);

/* Alignment of memory. */
size_t memalignment(const void *p);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STDLIB_H */
