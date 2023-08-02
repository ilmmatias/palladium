/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef STDIO_H
#define STDIO_H

#include <stdarg.h>

int sprintf(char *buffer, const char *format, ...);

int vsprintf(char *buffer, const char *format, va_list vlist);

#endif /* STDIO_H */
