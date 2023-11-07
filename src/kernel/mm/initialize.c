/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <mi.h>
#include <rt.h>

extern MiPageEntry *MiPageList;
extern MiPageEntry *MiFreePageListHead;

extern uint64_t MiPoolStart;
extern RtBitmap MiPoolBitmap;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the physical page allocator (and the page database).
 *
 * PARAMETERS:
 *     BootData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePageAllocator(LoaderBootData *BootData) {
    MiPageEntry *Tail = NULL;

    MiPageList = (MiPageEntry *)BootData->MemoryManager.PageAllocatorBase;

    for (uint32_t i = 0; i < BootData->MemoryMap.Count; i++) {
        BootMemoryRegion *Region = &BootData->MemoryMap.Entries[i];

        /* Available and `boot manager used` are considered the same for us (free for usage after
           we save required data from bootmgr), while anything else is considered reserved. */
        if (Region->Type != BOOT_MEMORY_REGION_TYPE_AVAILABLE &&
            Region->Type != BOOT_MEMORY_REGION_TYPE_USED) {
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

        if (Tail && Tail->GroupBase + (Tail->GroupPages << MM_PAGE_SHIFT) == Region->BaseAddress) {
            Tail->GroupPages += Region->Length >> MM_PAGE_SHIFT;
            continue;
        }

        MiPageEntry *Group = &MiPageList[Region->BaseAddress >> MM_PAGE_SHIFT];

        Group->References = 0;
        Group->GroupBase = Region->BaseAddress;
        Group->GroupPages = Region->Length >> MM_PAGE_SHIFT;
        Group->NextGroup = NULL;
        Group->PreviousGroup = Tail;

        if (Tail) {
            Tail->NextGroup = Group;
        } else {
            MiFreePageListHead = Group;
        }

        Tail = Group;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the kernel pool allocator.
 *
 * PARAMETERS:
 *     BootData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePool(LoaderBootData *BootData) {
    uint64_t Size = (MI_POOL_SIZE + MM_PAGE_SIZE) >> MM_PAGE_SHIFT;

    MiPoolStart = MI_POOL_START;

    RtInitializeBitmap(&MiPoolBitmap, (uint64_t *)BootData->MemoryManager.PoolBitmapBase, Size);
    RtClearAllBits(&MiPoolBitmap);
}
