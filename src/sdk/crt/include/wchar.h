/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef WCHAR_H
#define WCHAR_H

#include <crt_impl/common.h>
#include <stdio.h>

#define __STDC_VERSION_WCHAR_H__ 202311L

#define WEOF (-1)

/* Wrappers allowed to evaluate the input arguments more than once. */
#define getwc(stream) (fgetwc((stream)))
#define putwc(ch, stream) (fputwc((ch), (stream)))

typedef __WINT_TYPE__ wint_t;
struct tm;

typedef struct {
    unsigned __placeholder;
} mbstate_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Formatted wide character input/output functions. */
int fwprintf(FILE *CRT_RESTRICT stream, const wchar_t *CRT_RESTRICT format, ...);
int fwscanf(FILE *CRT_RESTRICT stream, const wchar_t *CRT_RESTRICT format, ...);
int swprintf(wchar_t *CRT_RESTRICT s, size_t n, const wchar_t *CRT_RESTRICT format, ...);
int swscanf(const wchar_t *CRT_RESTRICT s, const wchar_t *CRT_RESTRICT format, ...);
int vfwprintf(FILE *CRT_RESTRICT stream, const wchar_t *CRT_RESTRICT format, va_list arg);
int vfwscanf(FILE *CRT_RESTRICT stream, const wchar_t *CRT_RESTRICT format, va_list arg);
int vswprintf(wchar_t *CRT_RESTRICT s, size_t n, const wchar_t *CRT_RESTRICT format, va_list arg);
int vswscanf(const wchar_t *CRT_RESTRICT s, const wchar_t *CRT_RESTRICT format, va_list arg);
int vwprintf(const wchar_t *CRT_RESTRICT format, va_list arg);
int vwscanf(const wchar_t *CRT_RESTRICT format, va_list arg);
int wprintf(const wchar_t *CRT_RESTRICT format, ...);
int wscanf(const wchar_t *CRT_RESTRICT format, ...);

/* Wide character input/output functions. */
wint_t fgetwc(FILE *stream);
wchar_t *fgetws(wchar_t *CRT_RESTRICT s, int n, FILE *CRT_RESTRICT stream);
wint_t fputwc(wchar_t c, FILE *stream);
int fputws(const wchar_t *CRT_RESTRICT s, FILE *CRT_RESTRICT stream);
int fwide(FILE *stream, int mode);
wint_t getwchar(void);
wint_t putwchar(wchar_t c);
wint_t ungetwc(wint_t c, FILE *stream);

/* General wide string utilities. */
double wcstod(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr);
float wcstof(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr);
long double wcstold(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr);
long int wcstol(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr, int base);
long long int wcstoll(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr, int base);
unsigned long int
wcstoul(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr, int base);
unsigned long long int
wcstoull(const wchar_t *CRT_RESTRICT nptr, wchar_t **CRT_RESTRICT endptr, int base);
wchar_t *wcscpy(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2);
wchar_t *wcsncpy(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2, size_t n);
wchar_t *wmemcpy(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2, size_t n);
wchar_t *wmemmove(wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcscat(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2);
wchar_t *wcsncat(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2, size_t n);
int wcscmp(const wchar_t *s1, const wchar_t *s2);
int wcscoll(const wchar_t *s1, const wchar_t *s2);
int wcsncmp(const wchar_t *s1, const wchar_t *s2, size_t n);
size_t wcsxfrm(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2, size_t n);
int wmemcmp(const wchar_t *s1, const wchar_t *s2, size_t n);
wchar_t *wcschr(const wchar_t *s, wchar_t c);
size_t wcscspn(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcspbrk(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsrchr(const wchar_t *s, wchar_t c);
size_t wcsspn(const wchar_t *s1, const wchar_t *s2);
wchar_t *wcsstr(wchar_t *s1, const wchar_t *s2);
wchar_t *
wcstok(wchar_t *CRT_RESTRICT s1, const wchar_t *CRT_RESTRICT s2, wchar_t **CRT_RESTRICT ptr);
wchar_t *wmemchr(const wchar_t *s, wchar_t c, size_t n);
size_t wcslen(const wchar_t *s);
wchar_t *wmemset(wchar_t *s, wchar_t c, size_t n);

/* Wide character time conversion functions. */
size_t wcsftime(
    wchar_t *CRT_RESTRICT s,
    size_t maxsize,
    const wchar_t *CRT_RESTRICT format,
    const struct tm *CRT_RESTRICT timeptr);

/* Extended multibyte/wide character conversion utilities. */
wint_t btowc(int c);
int wctob(wint_t c);
int mbsinit(const mbstate_t *ps);
size_t mbrlen(const char *CRT_RESTRICT s, size_t n, mbstate_t *CRT_RESTRICT ps);
size_t mbrtowc(
    wchar_t *CRT_RESTRICT pwc,
    const char *CRT_RESTRICT s,
    size_t n,
    mbstate_t *CRT_RESTRICT ps);
size_t wcrtomb(char *CRT_RESTRICT s, wchar_t wc, mbstate_t *CRT_RESTRICT ps);
size_t mbsrtowcs(
    wchar_t *CRT_RESTRICT dst,
    const char **CRT_RESTRICT src,
    size_t len,
    mbstate_t *CRT_RESTRICT ps);
size_t wcsrtombs(
    char *CRT_RESTRICT dst,
    const wchar_t **CRT_RESTRICT src,
    size_t len,
    mbstate_t *CRT_RESTRICT ps);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* WCHAR_H */
