/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef RT_H
#define RT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint32_t RtGetHash(const void *Buffer, size_t Size);

#ifdef __cplusplus
}
#endif

#endif /* RT_H */
