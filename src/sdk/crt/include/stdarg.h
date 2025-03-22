/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDARG_H
#define STDARG_H

#define __STDC_VERSION_STDARG_H__ 202311L

/* Variable argument list access macros. */
#define va_arg(ap, type) __builtin_va_arg(ap, type)
#define va_copy(dest, src) __builtin_va_copy(dest, src)
#define va_end(ap) __builtin_va_end(ap)
#define va_start(ap, ...) __builtin_va_start(ap, __VA_ARGS__)

typedef __builtin_va_list va_list;

#endif /* STDARG_H */
