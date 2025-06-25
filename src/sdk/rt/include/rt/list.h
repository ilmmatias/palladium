/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _RT_LIST_H_
#define _RT_LIST_H_

#include <stddef.h>
#include <stdint.h>

#ifndef CONTAINING_RECORD
#define CONTAINING_RECORD(address, type, field) \
    ((type *)((char *)(address) - (uintptr_t)(&((type *)0)->field)))
#endif /* CONTAINING_RECORD */

typedef struct RtSList {
    struct RtSList *Next;
} RtSList;

typedef struct RtDList {
    struct RtDList *Next, *Prev;
} RtDList;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void RtPushSList(RtSList *Head, RtSList *Entry);
void RtAtomicPushSList(RtSList *Head, RtSList *Entry);
void RtSpliceSList(RtSList *Target, RtSList *Source);
RtSList *RtPopSList(RtSList *Head);

void RtInitializeDList(RtDList *Head);
void RtPushDList(RtDList *Head, RtDList *Entry);
void RtAppendDList(RtDList *Head, RtDList *Entry);
void RtSpliceHeadDList(RtDList *Target, RtDList *Source);
void RtSpliceTailDList(RtDList *Target, RtDList *Source);
RtDList *RtPopDList(RtDList *Head);
RtDList *RtTruncateDList(RtDList *Head);
void RtUnlinkDList(RtDList *Entry);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RT_LIST_H_ */
