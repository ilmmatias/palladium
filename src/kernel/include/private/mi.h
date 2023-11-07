/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _MI_H_
#define _MI_H_

#include <boot.h>
#include <mm.h>

#ifdef ARCH_amd64
#define MI_POOL_START 0xFFFF908000000000
#define MI_POOL_SIZE 0x2000000000
#else
#error "Undefined ARCH for the kernel module!"
#endif /* ARCH */

#define MI_MAP_WRITE 0x01
#define MI_MAP_EXEC 0x02

typedef struct MiPageEntry {
    uint64_t GroupBase;
    uint32_t GroupPages;

    struct {
        uint8_t References : 8;
        uint8_t StartOfAllocation : 1;
        uint8_t EndOfAllocation : 1;
    };

    struct MiPageEntry *NextGroup;
    struct MiPageEntry *PreviousGroup;
} MiPageEntry;

void MiInitializePageAllocator(LoaderBootData *BootData);
void MiInitializePool(LoaderBootData *BootData);

#endif /* _MI_H_ */
