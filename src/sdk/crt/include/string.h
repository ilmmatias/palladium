/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t strlen(const char *s);

void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

#ifdef __cplusplus
}
#endif

#endif /* STRING_H */
