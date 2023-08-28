/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _MM_H_
#define _MM_H_

#include <stdint.h>

#ifdef ARCH_amd64
#define MM_PAGE_SHIFT 12
#define MI_PADDR_TO_VADDR(Page) ((void *)((Page) + 0xFFFF800000000000))
#endif

#define MM_PAGE_SIZE (1ull << (MM_PAGE_SHIFT))

typedef struct MmPageEntry {
    uint8_t References;
    uint64_t GroupBase;
    uint32_t GroupPages;
    struct MmPageEntry *NextGroup;
    struct MmPageEntry *PreviousGroup;
} MmPageEntry;

void MiPreparePageAllocator(void *LoaderData);

uint64_t MmAllocatePages(uint32_t Pages);

#endif /* _MM_H_ */
