/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <memory.h>

extern BiMemoryDescriptor BiMemoryDescriptors[BI_MAX_MEMORY_DESCRIPTORS];
extern int BiMemoryDescriptorCount;

uint64_t BiUsableMemorySize = 0;
uint64_t BiUnusableMemorySize = 0;
uint64_t BiMaxAdressableMemory = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates the memory limit(s) for the system; This includes counting
 *     addressable memory, loader+kernel usable memory, etc.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiCalculateMemoryLimits(void) {
    for (int i = 0; i < BiMemoryDescriptorCount; i++) {
        BiMemoryDescriptor *Region = &BiMemoryDescriptors[i];

        /* Counting gaps as memory we shouldn't touch (unless the firmware passes some pointer
           or struct using it). */
        if (Region->Base != BiMaxAdressableMemory) {
            BiUnusableMemorySize += Region->Base - BiMaxAdressableMemory;
        }

        if (Region->Type == BM_MD_FREE) {
            BiUsableMemorySize += Region->Size;
        } else {
            /* We still shouldn't have any loader/kernel allocations. */
            BiUnusableMemorySize += Region->Size;
        }

        /* Regions are sorted by their base address, we can be sure anything we process will be
           larger than the previous item. */
        BiMaxAdressableMemory = Region->Base + Region->Size;
    }
}
