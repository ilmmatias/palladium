/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef UCHAR_H
#define UCHAR_H

#include <stddef.h>
#include <wchar.h>

#define __STDC_VERSION_UCHAR_H__ 202311L

typedef unsigned char char8_t;
typedef __UINT_LEAST16_TYPE__ char16_t;
typedef __UINT_LEAST32_TYPE__ char32_t;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Restartable multibyte/wide character conversion functions. */
size_t mbrtoc8(char8_t *restrict pc8, const char *restrict s, size_t n, mbstate_t *restrict ps);
size_t c8rtomb(char *restrict s, char8_t c8, mbstate_t *restrict ps);
size_t mbrtoc16(char16_t *restrict pc16, const char *restrict s, size_t n, mbstate_t *restrict ps);
size_t c16rtomb(char *restrict s, char16_t c16, mbstate_t *restrict ps);
size_t mbrtoc32(char32_t *restrict pc32, const char *restrict s, size_t n, mbstate_t *restrict ps);
size_t c32rtomb(char *restrict s, char32_t c32, mbstate_t *restrict ps);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* UCHAR_H */
