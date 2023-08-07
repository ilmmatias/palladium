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

#define FOPEN_MAX 1024
#define FILENAME_MAX 256

#define BUFSIZ 1024
#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define getc(stream) (fgetc((stream)))

typedef struct FILE FILE;

FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fclose(FILE *stream);
void setbuf(FILE *stream, char *buffer);
void setvbuf(FILE *stream, char *buffer, int mode, size_t size);

int fgetc(FILE *stream);
char *fgets(char *str, int count, FILE *stream);
size_t fread(void *buffer, size_t size, size_t count, FILE *stream);

int putchar(int ch);
int puts(const char *str);
int ungetc(int ch, FILE *stream);

int sscanf(char *buffer, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);

int vsscanf(char *buffer, const char *format, va_list vlist);
int vfscanf(FILE *stream, const char *format, va_list vlist);

int printf(const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t bufsz, const char *format, ...);

int vprintf(const char *format, va_list vlist);
int vsprintf(char *buffer, const char *format, va_list vlist);
int vsnprintf(char *buffer, size_t bufsz, const char *format, va_list vlist);

#endif /* STDIO_H */
