/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>
#include <mi.h>

extern MiPageEntry *MiPageList;
extern MiPageEntry *MiFreePageListHead;
extern MiPageEntry *MiFreePageListTail;

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
void MiInitializePageAllocator(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;

    MiPageList = (MiPageEntry *)BootData->MemoryManager.PageAllocatorBase;

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

        if (MiFreePageListTail &&
            MiFreePageListTail->GroupBase + (MiFreePageListTail->GroupPages << MM_PAGE_SHIFT) ==
                Region->BaseAddress) {
            MiFreePageListTail->GroupPages += Region->Length >> MM_PAGE_SHIFT;
            continue;
        }

        MiPageEntry *Group = &MiPageList[Region->BaseAddress >> MM_PAGE_SHIFT];

        Group->References = 0;
        Group->GroupBase = Region->BaseAddress;
        Group->GroupPages = Region->Length >> MM_PAGE_SHIFT;
        Group->NextGroup = NULL;
        Group->PreviousGroup = MiFreePageListTail;

        if (MiFreePageListTail) {
            MiFreePageListTail->NextGroup = Group;
        } else {
            MiFreePageListHead = Group;
        }

        MiFreePageListTail = Group;
    }
}
