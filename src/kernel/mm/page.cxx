/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <mi.h>
#include <vid.h>

#include <cxx/lock.hxx>

extern "C" {
MiPageEntry *MiPageList = NULL;
RtDList MiFreePageListHead;
KeSpinLock MiPageListLock = {0};
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries allocating a free physical memory page.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Physical address of the allocated page, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
extern "C" uint64_t MmAllocateSinglePage() {
    SpinLockGuard Guard(&MiPageListLock);

    RtDList *ListHeader = RtPopDList(&MiFreePageListHead);
    if (ListHeader == &MiFreePageListHead) {
        return 0;
    }

    MiPageEntry *Entry = CONTAINING_RECORD(ListHeader, MiPageEntry, ListHeader);
    if (Entry->Flags & MI_PAGE_FLAGS_USED) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER);
    }

    Entry->Flags = MI_PAGE_FLAGS_USED;
    return MI_PAGE_BASE(Entry);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the specified physical memory page to the free list.
 *
 * PARAMETERS:
 *     PhysicalAddress - The physical page address.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
extern "C" void MmFreeSinglePage(uint64_t PhysicalAddress) {
    SpinLockGuard Guard(&MiPageListLock);
    MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);

    if (!(Entry->Flags & MI_PAGE_FLAGS_USED) ||
        (Entry->Flags & (MI_PAGE_FLAGS_CONTIG_ANY | MI_PAGE_FLAGS_POOL_ANY))) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER);
    }

    Entry->Flags = 0;
    RtPushDList(&MiFreePageListHead, &Entry->ListHeader);
}
