/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _MI_H_
#define _MI_H_

#include <mm.h>

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

void MiInitializePageAllocator(void *LoaderData);

void MiInitializeVirtualMemory(void *LoaderData);
uint64_t MiGetPhysicalAddress(void *VirtualAddress);
int MiMapPage(void *VirtualAddress, uint64_t PhysicalAddress, int Flags);

#endif /* _MI_H_ */
