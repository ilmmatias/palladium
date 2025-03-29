/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef UCHAR_H
#define UCHAR_H

#include <crt_impl/common.h>
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
size_t mbrtoc8(
    char8_t *CRT_RESTRICT pc8,
    const char *CRT_RESTRICT s,
    size_t n,
    mbstate_t *CRT_RESTRICT ps);
size_t c8rtomb(char *CRT_RESTRICT s, char8_t c8, mbstate_t *CRT_RESTRICT ps);
size_t mbrtoc16(
    char16_t *CRT_RESTRICT pc16,
    const char *CRT_RESTRICT s,
    size_t n,
    mbstate_t *CRT_RESTRICT ps);
size_t c16rtomb(char *CRT_RESTRICT s, char16_t c16, mbstate_t *CRT_RESTRICT ps);
size_t mbrtoc32(
    char32_t *CRT_RESTRICT pc32,
    const char *CRT_RESTRICT s,
    size_t n,
    mbstate_t *CRT_RESTRICT ps);
size_t c32rtomb(char *CRT_RESTRICT s, char32_t c32, mbstate_t *CRT_RESTRICT ps);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* UCHAR_H */
