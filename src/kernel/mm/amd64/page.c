/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>
#include <mm.h>
#include <stddef.h>

extern MmPageEntry *MmPageList;
extern MmPageEntry *MmFreePageListHead;
extern MmPageEntry *MmFreePageListTail;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the architecture-dependent page allocator bits, getting ready to do
 *     physical page allocations.
 *
 * PARAMETERS:
 *     LoaderData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiPreparePageAllocator(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;

    MmPageList = (MmPageEntry *)BootData->MemoryManager.PageAllocatorBase;

    for (uint32_t i = 0; i < BootData->MemoryMap.Count; i++) {
        BiosMemoryRegion *Region = &BootData->MemoryMap.Entries[i];

        /* Available and `boot manager used` are considered the same for us (free for usage after
           we save required data from bootmgr), while anything else is considered reserved. */
        if (Region->Type != BIOS_MEMORY_REGION_TYPE_AVAILABLE &&
            Region->Type != BIOS_MEMORY_REGION_TYPE_USED) {
            continue;
        }

        /* The low 64KiB of memory are either marked as `boot manager used` or as
           `system reserved`; For the first case, we do need to make sure we don't add it to the
           free list. */
        if (Region->BaseAddress < 0x10000) {
            if (Region->BaseAddress + Region->Length < 0x10000) {
                continue;
            }

            Region->Length -= 0x10000 - Region->BaseAddress;
            Region->BaseAddress += 0x10000 - Region->BaseAddress;
        }

        /* The memory map should have been sorted by the boot manager, so we only have two options:
                We either need to append to the end of the free list,
                Or we need to extend the last entry. */

        if (MmFreePageListTail &&
            MmFreePageListTail->GroupBase + (MmFreePageListTail->GroupPages << MM_PAGE_SHIFT) ==
                Region->BaseAddress) {
            MmFreePageListTail->GroupPages += Region->Length >> MM_PAGE_SHIFT;
            continue;
        }

        MmPageEntry *Group = &MmPageList[Region->BaseAddress >> MM_PAGE_SHIFT];

        Group->References = 0;
        Group->GroupBase = Region->BaseAddress;
        Group->GroupPages = Region->Length >> MM_PAGE_SHIFT;
        Group->NextGroup = NULL;
        Group->PreviousGroup = MmFreePageListTail;

        if (MmFreePageListTail) {
            MmFreePageListTail->NextGroup = Group;
        } else {
            MmFreePageListHead = Group;
        }

        MmFreePageListTail = Group;
    }
}
