/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RT_HASH_H_
#define _RT_HASH_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint32_t RtGetHash(const void *Buffer, size_t Size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_HASH_H_ */
