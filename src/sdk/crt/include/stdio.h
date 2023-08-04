/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef STDIO_H
#define STDIO_H

#define __CRT_STDIO_H
#include <crt_impl.h>
#include <stdarg.h>
#include <stddef.h>
#undef __CRT_STDIO_H

#define EOF (-1)

typedef struct FILE FILE;

FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fclose(struct FILE *stream);

size_t fread(void *buffer, size_t size, size_t count, FILE *stream);

int putchar(int ch);
int puts(const char *str);

int printf(const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t bufsz, const char *format, ...);

int vprintf(const char *format, va_list vlist);
int vsprintf(char *buffer, const char *format, va_list vlist);
int vsnprintf(char *buffer, size_t bufsz, const char *format, va_list vlist);

#endif /* STDIO_H */
