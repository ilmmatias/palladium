/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include <stddef.h>
#include <stdint.h>

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define PAGE_SHIFT 12
#define ARENA_BASE 0xFFFF808000000000
#define ARENA_BITS 39
#else
#error "Undefined ARCH for the bootmgr module!"
#endif /* ARCH */

#define PAGE_SIZE (1ul << (PAGE_SHIFT))
#define ARENA_SIZE (1ull << (ARENA_BITS))

#define MEMORY_BOOT 0
#define MEMORY_KERNEL 1

typedef struct MemoryArena {
    uint64_t Base;
    uint64_t Size;
    struct MemoryArena *Next;
} MemoryArena;

extern MemoryArena *BmMemoryArena;

void BmInitMemory(void *BootBlock);
void *BmAllocatePages(size_t Pages, int Type);
void BmFreePages(void *Base, size_t Pages);

uint64_t BmAllocateVirtualAddress(uint64_t Pages);

#endif /* _MEMORY_H_ */
