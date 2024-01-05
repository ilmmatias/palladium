/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <memory.h>
#include <string.h>

extern BiMemoryDescriptor BiMemoryDescriptors[BI_MAX_MEMORY_DESCRIPTORS];
extern int BiMemoryDescriptorCount;

uint64_t BiMemoryUsedByLoader = 0;
uint64_t BiMemoryUsedByKernel = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates physical pages for use by the boot manager.
 *
 * PARAMETERS:
 *     Pages - How many bytes to allocate; This will get rounded up based on the page size.
 *     Type - Type of the allocation.
 *
 * RETURN VALUE:
 *     Base address of the allocation, or NULL if there is no memory left.
 *-----------------------------------------------------------------------------------------------*/
void *BmAllocatePages(uint64_t Size, int Type) {
    Size = ((Size + BI_PAGE_SIZE - 1) >> BI_PAGE_SHIFT) << BI_PAGE_SHIFT;

    if (Type < BM_MD_BOOTMGR) {
        Type = BM_MD_BOOTMGR;
    }

    for (int i = 0; i < BiMemoryDescriptorCount; i++) {
        BiMemoryDescriptor *Region = &BiMemoryDescriptors[i];

        if (Region->Type != BM_MD_FREE || Region->Size < Size) {
            continue;
        }

        /* We have three cases to take into consideration: Exact match (consuming
         * the rest of this entry); Just flip the type to USED. End of the list; This entry
         * becomes USED, and takes the exact size of the allocated region; A new region is
         * added to the end, containing the remaining AVAILABLE memory. Middle of the list;
         * We need to memmove the entire list first (this is the TODO/to optimize part),
         * before executing the same procedure as above. */

        if (Region->Size == Size) {
            Region->Type = Type;

            if (Type == BM_MD_BOOTMGR) {
                BiMemoryUsedByLoader += Size;
            } else {
                BiMemoryUsedByKernel += Size;
            }

            return (void *)Region->Base;
        } else if (BiMemoryDescriptorCount >= BI_MAX_MEMORY_DESCRIPTORS) {
            /* We're out of memory descriptors, bail out. */
            return NULL;
        } else if (i != BiMemoryDescriptorCount - 1) {
            memmove(
                Region + 2,
                Region + 1,
                (BiMemoryDescriptorCount - i - 1) * sizeof(BiMemoryDescriptor));
        }

        BiMemoryDescriptor *NewRegion = Region + 1;

        NewRegion->Type = BM_MD_FREE;
        NewRegion->Base = Region->Base + Size;
        NewRegion->Size = Region->Size - Size;

        Region->Type = Type;
        Region->Size = Size;

        BiMemoryDescriptorCount++;
        if (Type == BM_MD_BOOTMGR) {
            BiMemoryUsedByLoader += Size;
        } else {
            BiMemoryUsedByKernel += Size;
        }

        return (void *)Region->Base;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the specified range of pages for use by the allocator.
 *
 * PARAMETERS:
 *     Base - Base address returned by BmAllocatePages.
 *     Size - How many bytes were originally allocated by BmAllocatePages.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmFreePages(void *Base, uint64_t Size) {
    Size = ((Size + BI_PAGE_SIZE - 1) >> BI_PAGE_SHIFT) << BI_PAGE_SHIFT;

    for (int i = 0; i < BiMemoryDescriptorCount; i++) {
        BiMemoryDescriptor *Region = &BiMemoryDescriptors[i];

        /* We want an exact match; You do need to pass the exact same amount of pages as you
           did for BmAllocatePages! */
        if (Region->Type != BM_MD_BOOTMGR || Region->Base != (uint64_t)Base ||
            Region->Size != Size) {
            continue;
        }

        /* Merge with neighbours to decrease fragmentation; First merge forward, then
           backwards. */
        Region->Type = BM_MD_FREE;
        if (Region->Type == BM_MD_BOOTMGR) {
            BiMemoryUsedByLoader -= Size;
        } else {
            BiMemoryUsedByKernel -= Size;
        }

        if (i + 1 < BiMemoryDescriptorCount && BiMemoryDescriptors[i + 1].Type == BM_MD_FREE &&
            BiMemoryDescriptors[i + 1].Base == Region->Base + Region->Size) {
            Region->Size += BiMemoryDescriptors[i + 1].Size;
            memmove(
                Region + 1,
                Region + 2,
                (BiMemoryDescriptorCount - i - 2) * sizeof(BiMemoryDescriptor));
            BiMemoryDescriptorCount--;
        }

        if (i > 0 && BiMemoryDescriptors[i - 1].Type == BM_MD_FREE &&
            BiMemoryDescriptors[i - 1].Base + BiMemoryDescriptors[i - 1].Size == Region->Base) {
            BiMemoryDescriptors[i - 1].Size += Region->Size;
            memmove(
                Region, Region + 1, (BiMemoryDescriptorCount - i - 1) * sizeof(BiMemoryDescriptor));
            BiMemoryDescriptorCount--;
        }

        break;
    }
}
