/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_ATOMIC_H_
#define _RT_ATOMIC_H_

#include <rt/list.h>

/* This needs to be aligned(8) for 32-bit systems, so, TODO. */
typedef struct __attribute__((aligned(16))) RtAtomicSList {
    RtSList *Next;
    uint64_t Tag;
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
