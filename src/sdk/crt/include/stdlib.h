/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef STDLIB_H
#define STDLIB_H

#include <limits.h>
#include <stddef.h>

#define RAND_MAX INT_MAX

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

double atof(const char *str);
int atoi(const char *str);
long atol(const char *str);
long long atoll(const char *str);
long strtol(const char *str, char **str_end, int base);
long long strtoll(const char *str, char **str_end, int base);
unsigned long strtoul(const char *str, char **str_end, int base);
unsigned long long strtoull(const char *str, char **str_end, int base);
float strtof(const char *str, char **str_end);
double strtod(const char *str, char **str_end);
long double strtold(const char *str, char **str_end);

void *malloc(size_t size);
void *calloc(size_t num, size_t size);
void free(void *ptr);

int rand(void);
void srand(unsigned seed);

void abort(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STDLIB_H */
