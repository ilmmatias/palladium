/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STRING_H
#define STRING_H

#include <crt_impl/common.h>
#include <stddef.h>

#define __STDC_VERSION_STRING_H__ 202311L

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Copying functions. */
void *memcpy(void *CRT_RESTRICT s1, const void *CRT_RESTRICT s2, size_t n);
void *memccpy(void *CRT_RESTRICT s1, const void *CRT_RESTRICT s2, int c, size_t n);
void *memmove(void *s1, const void *s2, size_t n);
char *strcpy(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2);
char *strncpy(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2, size_t n);
char *strdup(const char *s);
char *strndup(const char *s, size_t n);

/* Concatenation functions. */
char *strcat(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2);
char *strncat(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2, size_t n);

/* Comparison functions. */
int memcmp(const void *s1, const void *s2, size_t n);
int strcmp(const char *s1, const char *s2);
int strcoll(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
size_t strxfrm(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2, size_t n);

/* Search functions. */
void *memchr(const void *s, int c, size_t n);
char *strchr(const char *s, int c);
size_t strcspn(const char *s1, const char *s2);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *s, int c);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s1, const char *s2);
char *strtok(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2);

/* Miscellaneous functions. */
void *memset(void *s, int c, size_t n);
void *memset_explicit(void *s, int c, size_t n);
char *strerror(int errnum);
size_t strlen(const char *s);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STRING_H */
