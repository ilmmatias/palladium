/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_ATOMIC_H_
#define _RT_ATOMIC_H_

#include <rt/list.h>

#if UINTPTR_MAX == UINT64_MAX
typedef struct __attribute__((aligned(16))) RtAtomicSList {
#else
typedef struct __attribute__((aligned(8))) RtAtomicSList {
#endif
    RtSList *Next;
    uintptr_t Tag;
} RtAtomicSList;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void RtPushAtomicSList(RtAtomicSList *Header, RtSList *Entry);
RtSList *RtPopAtomicSList(RtAtomicSList *Header);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_ATOMIC_H_ */
