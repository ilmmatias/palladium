/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef CRT_IMPL_RAND_H
#define CRT_IMPL_RAND_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint64_t __rand64(void);
void __srand64(uint64_t seed);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CRT_IMPL_RAND_H */
