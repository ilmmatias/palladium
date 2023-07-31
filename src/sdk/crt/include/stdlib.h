/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void *malloc(size_t size);
void *calloc(size_t num, size_t size);
void free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* STDLIB_H */
