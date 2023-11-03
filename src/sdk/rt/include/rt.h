/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef RT_H
#define RT_H

#include <stddef.h>
#include <stdint.h>

#define CONTAINING_RECORD(address, type, field) \
    ((type *)((char *)(address) - (uintptr_t)(&((type *)0)->field)))

typedef struct RtSList {
    struct RtSList *Next;
} RtSList;

typedef struct RtDList {
    struct RtDList *Next, *Prev;
} RtDList;

typedef struct {
    uint64_t *Buffer;
    size_t NumberOfBits;
} RtBitmap;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

uint32_t RtGetHash(const void *Buffer, size_t Size);

void RtPushSList(RtSList *Head, RtSList *Entry);
RtSList *RtPopSList(RtSList *Head);

void RtInitializeDList(RtDList *Head);
void RtPushDList(RtDList *Head, RtDList *Entry);
void RtAppendDList(RtDList *Head, RtDList *Entry);
RtDList *RtPopDList(RtDList *Head);
RtDList *RtTruncateDList(RtDList *Head);
void RtUnlinkDList(RtDList *Entry);

void RtInitializeBitmap(RtBitmap *Header, uint64_t *Buffer, uint64_t NumberOfBits);
void RtClearBit(RtBitmap *Header, uint64_t Bit);
void RtClearBits(RtBitmap *Header, uint64_t Start, uint64_t NumberOfBits);
void RtClearAllBits(RtBitmap *Header);
void RtSetBit(RtBitmap *Header, uint64_t Bit);
void RtSetBits(RtBitmap *Header, uint64_t Start, uint64_t NumberOfBits);
void RtSetAllBits(RtBitmap *Header);
uint64_t RtFindClearBits(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits);
uint64_t RtFindClearBitsAndSet(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits);
uint64_t RtFindSetBits(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits);
uint64_t RtFindSetBitsAndClear(RtBitmap *Header, uint64_t Hint, uint64_t NumberOfBits);

#ifdef __cplusplus
}
#endif

#endif /* RT_H */
