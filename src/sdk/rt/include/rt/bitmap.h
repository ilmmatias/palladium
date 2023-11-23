/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _RT_BITMAP_H_
#define _RT_BITMAP_H_

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint64_t *Buffer;
    size_t NumberOfBits;
} RtBitmap;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
#endif /* __cplusplus */

#endif /* _RT_BITMAP_H_ */
