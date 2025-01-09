/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <mi.h>
#include <vid.h>

#include <cxx/lock.hxx>

extern "C" {
MiPageEntry *MiPageList = NULL;
MiPageEntry *MiFreePageListHead = NULL;
}

static KeSpinLock PageListLock = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a single physical page; We need to add back a contig allocation
 *     function, ASAP.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Physical address of the allocated page, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
extern "C" uint64_t MmAllocatePage(void) {
    SpinLockGuard Guard(&PageListLock);

    if (MiFreePageListHead) {
        MiPageEntry *Page = MiFreePageListHead;
        MiFreePageListHead = Page->NextPage;

        if (Page->References != 0) {
            KeFatalError(KE_BAD_POOL_HEADER);
        }

        Page->References = 1;
        return MI_PAGE_BASE(Page);
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tells the memory manager we'll use the specified physical memory page.
 *
 * PARAMETERS:
 *     PhysicalAddress - The address we'll be using.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
extern "C" void MmReferencePage(uint64_t PhysicalAddress) {
    SpinLockGuard Guard(&PageListLock);
    if (MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].References != UINT32_MAX) {
        MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].References++;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tells the memory manager were done using the specified physical page, and it
 *     can return it to the free list if no one else is using it.
 *
 * PARAMETERS:
 *     PhysicalAddress - The physical page address.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
extern "C" void MmDereferencePage(uint64_t PhysicalAddress) {
    SpinLockGuard Guard(&PageListLock);
    MiPageEntry *Entry = &MiPageList[PhysicalAddress >> MM_PAGE_SHIFT];

    if (!Entry->References--) {
        KeFatalError(KE_DOUBLE_PAGE_FREE);
    }

    if (Entry->References) {
        return;
    }

    Entry->NextPage = MiFreePageListHead;
    MiFreePageListHead = Entry;
}
