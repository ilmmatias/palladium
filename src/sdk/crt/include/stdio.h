/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDIO_H
#define STDIO_H

#include <crt_impl/file.h>
#include <stdarg.h>
#include <stddef.h>

#define EOF (-1)

#define FOPEN_MAX 1024
#define FILENAME_MAX 1024
#define TMP_MAX 1024
#define L_tmpnam 1024
#define _PRINTF_NAN_LEN_MAX 4

#define BUFSIZ 1024
#define _IONBF 0
#define _IOLBF 1
#define _IOFBF 2

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* Wrappers allowed to evaluate the input arguments more than once. */
#define getc_unlocked(stream) (fgetc_unlocked((stream)))
#define putc_unlocked(ch, stream) (fputc_unlocked((ch), (stream)))
#define getc(stream) (fgetc((stream)))
#define putc(ch, stream) (fputc((ch), (stream)))

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Standard streams. */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Operations on files. */
int remove(const char *filename);
int rename(const char *old, const char *new);
FILE *tmpfile(void);
char *tmpnam(char *s);

/* File access functions (unlocked variants). */
int fflush_unlocked(FILE *stream);
void setbuf_unlocked(FILE *restrict stream, char *restrict buf);
int setvbuf_unlocked(FILE *restrict stream, char *restrict buf, int mode, size_t size);

/* File access functions. */
int fclose(FILE *stream);
int fflush(FILE *stream);
FILE *fopen(const char *restrict filename, const char *restrict mode);
FILE *freopen(const char *restrict filename, const char *restrict mode, FILE *restrict stream);
void setbuf(FILE *restrict stream, char *restrict buf);
int setvbuf(FILE *restrict stream, char *restrict buf, int mode, size_t size);

/* Formatted input/output functions (unlocked variants). */
int fprintf_unlocked(FILE *restrict stream, const char *restrict format, ...);
int fscanf_unlocked(FILE *restrict stream, const char *restrict format, ...);
int printf_unlocked(const char *restrict format, ...);
int scanf_unlocked(const char *restrict format, ...);
int vfprintf_unlocked(FILE *restrict stream, const char *restrict format, va_list arg);
int vfscanf_unlocked(FILE *restrict stream, const char *restrict format, va_list arg);
int vprintf_unlocked(const char *restrict format, va_list arg);
int vscanf_unlocked(const char *restrict format, va_list arg);

/* Formatted input/output functions. */
int fprintf(FILE *restrict stream, const char *restrict format, ...);
int fscanf(FILE *restrict stream, const char *restrict format, ...);
int printf(const char *restrict format, ...);
int scanf(const char *restrict format, ...);
int snprintf(char *restrict s, size_t n, const char *restrict format, ...);
int sprintf(char *restrict s, const char *restrict format, ...);
int sscanf(const char *restrict s, const char *restrict format, ...);
int vfprintf(FILE *restrict stream, const char *restrict format, va_list arg);
int vfscanf(FILE *restrict stream, const char *restrict format, va_list arg);
int vprintf(const char *restrict format, va_list arg);
int vscanf(const char *restrict format, va_list arg);
int vsnprintf(char *restrict s, size_t n, const char *restrict format, va_list arg);
int vsprintf(char *restrict s, const char *restrict format, va_list arg);
int vsscanf(const char *restrict s, const char *restrict format, va_list arg);

/* Character input/output functions (unlocked variants). */
int fgetc_unlocked(FILE *stream);
char *fgets_unlocked(char *restrict s, int n, FILE *restrict stream);
int fputc_unlocked(int c, FILE *stream);
int fputs_unlocked(const char *restrict s, FILE *restrict stream);
int getchar_unlocked(void);
int putchar_unlocked(int c);
int puts_unlocked(const char *s);
int ungetc_unlocked(int c, FILE *stream);

/* Character input/output functions. */
int fgetc(FILE *stream);
char *fgets(char *restrict s, int n, FILE *restrict stream);
int fputc(int c, FILE *stream);
int fputs(const char *restrict s, FILE *restrict stream);
int getchar(void);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);

/* Direct input/output functions (unlocked variants). */
size_t fread_unlocked(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream);
size_t fwrite_unlocked(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream);
int fgetpos_unlocked(FILE *restrict stream, fpos_t *restrict pos);
int fseek_unlocked(FILE *stream, long int offset, int whence);
int fsetpos_unlocked(FILE *stream, const fpos_t *pos);
long int ftell_unlocked(FILE *stream);
void rewind_unlocked(FILE *stream);

/* Direct input/output functions. */
size_t fread(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream);
int fgetpos(FILE *restrict stream, fpos_t *restrict pos);
int fseek(FILE *stream, long int offset, int whence);
int fsetpos(FILE *stream, const fpos_t *pos);
long int ftell(FILE *stream);
void rewind(FILE *stream);

/* Error-handling functions (unlocked variants). */
void clearerr_unlocked(FILE *stream);
int feof_unlocked(FILE *stream);
int ferror_unlocked(FILE *stream);

/* Error-handling functions. */
void clearerr(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
void perror(const char *s);

/* Thread-safety helper functions. */
void flockfile(FILE *stream);
void funlockfile(FILE *stream);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STDIO_H */
