/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_FMT_H
#define CRT_IMPL_FMT_H

#include <stdarg.h>
#include <stdint.h>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

int __vwprintf(
    const wchar_t *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

int __vscanf(
    const char *format,
    va_list vlist,
    void *context,
    int (*read_ch)(void *context),
    void (*unread_ch)(void *context, int ch));

int __vwscanf(
    const wchar_t *format,
    va_list vlist,
    void *context,
    int (*read_ch)(void *context),
    void (*unread_ch)(void *context, int ch));

double __strtod_hex(const char *str, double sign);
double __strtod_dec(const char *str, double sign);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_IMPL_FMT_H */
