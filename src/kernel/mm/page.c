/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ke.h>
#include <mi.h>

MiPageEntry *MiPageList = NULL;
MiPageEntry *MiFreePageListHead = NULL;

static MiPageEntry *DeferredFreePageListHead = NULL;
static int DeferredFreePageListSize = 0;

static KeSpinLock PageListLock = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function merges us with all neighbouring groups (if possible).
 *
 * PARAMETERS:
 *     Group - Starting point of the merge
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void Merge(MiPageEntry *Group) {
    while (Group->NextGroup &&
           Group->GroupBase + (Group->GroupPages << MM_PAGE_SHIFT) == Group->NextGroup->GroupBase) {
        Group->GroupPages += Group->NextGroup->GroupPages;
        Group->NextGroup = Group->NextGroup->NextGroup;
    }

    while (Group->PreviousGroup &&
           Group->PreviousGroup->GroupBase + (Group->PreviousGroup->GroupPages << MM_PAGE_SHIFT) ==
               Group->GroupBase) {
        Group->GroupBase = Group->PreviousGroup->GroupBase;
        Group->GroupPages += Group->PreviousGroup->GroupPages;
        Group->PreviousGroup = Group->PreviousGroup->PreviousGroup;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends all pages from the deferred free list back into the main free list.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void DeferredFreePages(void) {
    DeferredFreePageListSize = 0;

    while (DeferredFreePageListHead) {
        MiPageEntry *Entry = DeferredFreePageListHead;
        DeferredFreePageListHead = Entry->NextGroup;

        /* The list should be always sorted by address; We want either a match where we'll extend
            the size/base, or we want to find where to insert this page. */
        MiPageEntry *Group = MiFreePageListHead;
        while (Group && Group->GroupBase < Entry->GroupBase) {
            Group = Group->NextGroup;
        }

        if (Group && Entry->GroupBase + MM_PAGE_SIZE == Group->GroupBase) {
            Group->GroupBase = Entry->GroupBase;
            Group->GroupPages++;
            Merge(Group);
            return;
        } else if (
            Group && Group->GroupBase + (Group->GroupPages << MM_PAGE_SHIFT) == Entry->GroupBase) {
            Group->GroupPages++;
            Merge(Group);
            return;
        }

        /* We somehow reached zero free pages, and got around freeing something, just setup the list
           again. */
        if (!Group) {
            Entry->NextGroup = NULL;
            MiFreePageListHead = Entry;
            return;
        }

        /* Otherwise, append before the last entry we saw (it is bigger than us!). */
        Entry->PreviousGroup = Group->PreviousGroup;
        Entry->NextGroup = Group;
        Group->PreviousGroup = Entry;

        if (Entry->PreviousGroup) {
            Entry->PreviousGroup->NextGroup = Entry;
        } else {
            MiFreePageListHead = Entry;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a free consecutive physical page range in memory, targeting to put
 *     it in the first possible address.
 *
 * PARAMETERS:
 *     Pages - How many pages should be allocated.
 *
 * RETURN VALUE:
 *     Physical address of the first allocated page, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t MmAllocatePages(uint32_t Pages) {
    if (!Pages) {
        Pages = 1;
    }

    KeIrql Irql = KeAcquireSpinLock(&PageListLock);

    /* Deferred free pages are always size 1, we can/should use them if possible. */
    if (DeferredFreePageListHead && Pages == 1) {
        MiPageEntry *Page = DeferredFreePageListHead;
        DeferredFreePageListHead = Page->NextGroup;
        DeferredFreePageListSize--;
        Page->References = 1;
        KeReleaseSpinLock(&PageListLock, Irql);
        return Page->GroupBase;
    }

    /* Two attempts, if we fail the first, return everything in the deferred list and try
        again. */
    int Retry = (DeferredFreePageListSize != 0) + 1;
    MiPageEntry *Group = NULL;

    do {
        Group = MiFreePageListHead;
        while (Group && Group->GroupPages < Pages) {
            Group = Group->NextGroup;
        }

        if (Group) {
            break;
        }

        Retry--;
        if (Retry) {
            DeferredFreePages();
        }
    } while (Retry);

    if (!Group) {
        KeReleaseSpinLock(&PageListLock, Irql);
        return 0;
    }

    /* On non perfectly sized matches, we can just update which entry is the group base. */

    if (Pages < Group->GroupPages) {
        MiPageEntry *Entry = Group + Pages;
        Entry->GroupBase = Group->GroupBase + (Pages << MM_PAGE_SHIFT);
        Entry->GroupPages = Group->GroupPages - Pages;
        Entry->PreviousGroup = Group->PreviousGroup;
        Entry->NextGroup = Group->NextGroup;

        if (Entry->PreviousGroup) {
            Entry->PreviousGroup->NextGroup = Entry;
        } else {
            MiFreePageListHead = Entry;
        }

        if (Entry->NextGroup) {
            Entry->NextGroup->PreviousGroup = Entry;
        }

        for (uint32_t i = 0; i < Pages; i++) {
            (Group + i)->References = 1;
        }

        KeReleaseSpinLock(&PageListLock, Irql);
        return Group->GroupBase;
    }

    /* Otherwise, we can just remove the current entry. */

    if (Group->PreviousGroup) {
        Group->PreviousGroup->NextGroup = Group->NextGroup;
    } else {
        MiFreePageListHead = Group->NextGroup;
    }

    if (Group->NextGroup) {
        Group->NextGroup->PreviousGroup = Group->PreviousGroup;
    }

    for (uint32_t i = 0; i < Pages; i++) {
        (Group + i)->References = 1;
    }

    KeReleaseSpinLock(&PageListLock, Irql);
    return Group->GroupBase;
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
void MmReferencePage(uint64_t PhysicalAddress) {
    KeIrql Irql = KeAcquireSpinLock(&PageListLock);

    if (MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].References != UINT8_MAX) {
        MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].References++;
    }

    KeReleaseSpinLock(&PageListLock, Irql);
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
void MmDereferencePage(uint64_t PhysicalAddress) {
    KeIrql Irql = KeAcquireSpinLock(&PageListLock);

    if (!MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].References--) {
        KeFatalError(KE_DOUBLE_PAGE_FREE);
    }

    if (MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].References) {
        KeReleaseSpinLock(&PageListLock, Irql);
        return;
    }

    MiPageEntry *Entry = &MiPageList[PhysicalAddress >> MM_PAGE_SHIFT];
    Entry->GroupBase = PhysicalAddress;
    Entry->GroupPages = 1;
    Entry->PreviousGroup = NULL;
    Entry->NextGroup = DeferredFreePageListHead;

    DeferredFreePageListHead = Entry;
    DeferredFreePageListSize++;

    if (DeferredFreePageListSize >= 32) {
        DeferredFreePages();
    }

    KeReleaseSpinLock(&PageListLock, Irql);
}
