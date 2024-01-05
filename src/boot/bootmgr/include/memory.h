/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define BI_RESERVED_BASE 0x1000
#define BI_RESERVED_SIZE 0x7F00

#define BI_BOOTSTRAP_IMAGE_BASE 0x9200
#define BI_SELF_IMAGE_BASE 0x9A00

#define BI_PAGE_SHIFT 12

#define BI_ARENA_PAGE_SHIFT 30
#define BI_ARENA_BASE 0xFFFF900000000000
#else
#error "Undefined ARCH for the bootmgr module!"
#endif

#define BI_PAGE_SIZE (1ull << BI_PAGE_SHIFT)
#define BI_ARENA_PAGE_SIZE (1ull << (BI_ARENA_PAGE_SHIFT))

#define BI_MAX_MEMORY_DESCRIPTORS 256

#define BM_MD_FREE 0
#define BM_MD_HARDWARE 1
#define BM_MD_BOOTMGR 2
#define BM_MD_KERNEL 3

typedef struct __attribute__((packed)) {
    int Type;
    uint64_t Base;
    uint64_t Size;
} BiMemoryDescriptor;

typedef struct BiMemoryArenaEntry {
    uint64_t Base;
    struct BiMemoryArenaEntry *Next;
} BiMemoryArenaEntry;

void BiInitializeMemory(void);
void BiCalculateMemoryLimits(void);
void BiAddMemoryDescriptor(int Type, uint64_t Base, uint64_t Size);

void *BmAllocatePages(uint64_t Size, int Type);
void BmFreePages(void *Base, uint64_t Size);

void *BmAllocateBlock(size_t Size);
void *BmAllocateZeroBlock(size_t Elements, size_t ElementSize);
void BmFreeBlock(void *Block);

uint64_t BmAllocateVirtualAddress(uint64_t Pages);

#endif /* _MEMORY_H_ */
