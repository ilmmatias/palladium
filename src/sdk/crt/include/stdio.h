/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDIO_H
#define STDIO_H

#include <crt_impl/common.h>
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
void setbuf_unlocked(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf);
int setvbuf_unlocked(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf, int mode, size_t size);

/* File access functions. */
int fclose(FILE *stream);
int fflush(FILE *stream);
CRT_MALLOC FILE *fopen(const char *CRT_RESTRICT filename, const char *CRT_RESTRICT mode);
FILE *freopen(
    const char *CRT_RESTRICT filename,
    const char *CRT_RESTRICT mode,
    FILE *CRT_RESTRICT stream);
void setbuf(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf);
int setvbuf(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf, int mode, size_t size);

/* Formatted input/output functions (unlocked variants). */
CRT_FORMAT(printf, 2, 3)
int fprintf_unlocked(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, ...);
CRT_FORMAT(scanf, 2, 3)
int fscanf_unlocked(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, ...);
CRT_FORMAT(printf, 1, 2) int printf_unlocked(const char *CRT_RESTRICT format, ...);
CRT_FORMAT(scanf, 1, 2) int scanf_unlocked(const char *CRT_RESTRICT format, ...);
int vfprintf_unlocked(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, va_list arg);
int vfscanf_unlocked(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, va_list arg);
int vprintf_unlocked(const char *CRT_RESTRICT format, va_list arg);
int vscanf_unlocked(const char *CRT_RESTRICT format, va_list arg);

/* Formatted input/output functions. */
CRT_FORMAT(printf, 2, 3)
int fprintf(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, ...);
CRT_FORMAT(scanf, 2, 3) int fscanf(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, ...);
CRT_FORMAT(printf, 1, 2) int printf(const char *CRT_RESTRICT format, ...);
CRT_FORMAT(scanf, 1, 2) int scanf(const char *CRT_RESTRICT format, ...);
CRT_FORMAT(printf, 3, 4)
int snprintf(char *CRT_RESTRICT s, size_t n, const char *CRT_RESTRICT format, ...);
CRT_FORMAT(printf, 2, 3) int sprintf(char *CRT_RESTRICT s, const char *CRT_RESTRICT format, ...);
CRT_FORMAT(scanf, 2, 3)
int sscanf(const char *CRT_RESTRICT s, const char *CRT_RESTRICT format, ...);
int vfprintf(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, va_list arg);
int vfscanf(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, va_list arg);
int vprintf(const char *CRT_RESTRICT format, va_list arg);
int vscanf(const char *CRT_RESTRICT format, va_list arg);
int vsnprintf(char *CRT_RESTRICT s, size_t n, const char *CRT_RESTRICT format, va_list arg);
int vsprintf(char *CRT_RESTRICT s, const char *CRT_RESTRICT format, va_list arg);
int vsscanf(const char *CRT_RESTRICT s, const char *CRT_RESTRICT format, va_list arg);

/* Character input/output functions (unlocked variants). */
int fgetc_unlocked(FILE *stream);
char *fgets_unlocked(char *CRT_RESTRICT s, int n, FILE *CRT_RESTRICT stream);
int fputc_unlocked(int c, FILE *stream);
int fputs_unlocked(const char *CRT_RESTRICT s, FILE *CRT_RESTRICT stream);
int getchar_unlocked(void);
int putchar_unlocked(int c);
int puts_unlocked(const char *s);
int ungetc_unlocked(int c, FILE *stream);

/* Character input/output functions. */
int fgetc(FILE *stream);
char *fgets(char *CRT_RESTRICT s, int n, FILE *CRT_RESTRICT stream);
int fputc(int c, FILE *stream);
int fputs(const char *CRT_RESTRICT s, FILE *CRT_RESTRICT stream);
int getchar(void);
int putchar(int c);
int puts(const char *s);
int ungetc(int c, FILE *stream);

/* Direct input/output functions (unlocked variants). */
size_t fread_unlocked(void *CRT_RESTRICT ptr, size_t size, size_t nmemb, FILE *CRT_RESTRICT stream);
size_t
fwrite_unlocked(const void *CRT_RESTRICT ptr, size_t size, size_t nmemb, FILE *CRT_RESTRICT stream);
int fgetpos_unlocked(FILE *CRT_RESTRICT stream, fpos_t *CRT_RESTRICT pos);
int fseek_unlocked(FILE *stream, long int offset, int whence);
int fsetpos_unlocked(FILE *stream, const fpos_t *pos);
long int ftell_unlocked(FILE *stream);
void rewind_unlocked(FILE *stream);

/* Direct input/output functions. */
size_t fread(void *CRT_RESTRICT ptr, size_t size, size_t nmemb, FILE *CRT_RESTRICT stream);
size_t fwrite(const void *CRT_RESTRICT ptr, size_t size, size_t nmemb, FILE *CRT_RESTRICT stream);
int fgetpos(FILE *CRT_RESTRICT stream, fpos_t *CRT_RESTRICT pos);
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
