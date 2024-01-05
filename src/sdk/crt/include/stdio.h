/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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
#define putc(ch, stream) (fputc((ch), (stream)))

typedef struct FILE FILE;
typedef struct fpos_t fpos_t;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

FILE *fopen(const char *filename, const char *mode);
FILE *freopen(const char *filename, const char *mode, FILE *stream);
int fclose(FILE *stream);
int fflush(FILE *stream);
void setbuf(FILE *stream, char *buffer);
void setvbuf(FILE *stream, char *buffer, int mode, size_t size);

size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream);

int fgetc(FILE *stream);
int fputc(int ch, FILE *stream);
char *fgets(char *str, int count, FILE *stream);
int fputs(const char *str, FILE *stream);
int getchar(void);
char *gets(char *str);
int putchar(int ch);
int puts(const char *str);

int getchar(void);
char *gets(char *str);
int putchar(int ch);
int puts(const char *str);
int ungetc(int ch, FILE *stream);

int scanf(const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);
int sscanf(const char *buffer, const char *format, ...);

int vscanf(const char *format, va_list vlist);
int vfscanf(FILE *stream, const char *format, va_list vlist);
int vsscanf(const char *buffer, const char *format, va_list vlist);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t bufsz, const char *format, ...);

int vprintf(const char *format, va_list vlist);
int vfprintf(FILE *stream, const char *format, va_list vlist);
int vsprintf(char *buffer, const char *format, va_list vlist);
int vsnprintf(char *buffer, size_t bufsz, const char *format, va_list vlist);

long ftell(FILE *stream);
int fgetpos(FILE *stream, fpos_t *pos);
int fseek(FILE *stream, long offset, int origin);
int fsetpos(FILE *stream, fpos_t *pos);
int rewind(FILE *stream);
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);

#endif /* STDIO_H */
