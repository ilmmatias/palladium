/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <mm.h>
#include <stddef.h>

MmPageEntry *MmPageList = NULL;
MmPageEntry *MmFreePageListHead = NULL;
MmPageEntry *MmFreePageListTail = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a free physical page range in memory, targeting to put it in the
 *     first possible address.
 *
 * PARAMETERS:
 *     Pages - How many pages should be allocated.
 *
 * RETURN VALUE:
 *     Physical address of the first allocated page, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t MmAllocatePages(uint32_t Pages) {
    MmPageEntry *Group = MmFreePageListHead;

    while (Group && Group->GroupPages < Pages) {
        Group = Group->NextGroup;
    }

    if (!Group) {
        return 0;
    }

    /* On non perfectly sized matches, we can just update the group base and size. */
    if (Pages < Group->GroupPages) {
        uint64_t Base = Group->GroupBase;
        Group->GroupBase += (uint64_t)Pages << MM_PAGE_SHIFT;
        Group->GroupPages -= Pages;
        return Base;
    }

    /* On perfect match, we have two options:
           - Set the size to zero, and leave it in place, waiting for clean up later.
           - Remove the group from the linked list; This is what we're doing below. */

    if (Group->PreviousGroup) {
        Group->PreviousGroup->NextGroup = Group->NextGroup;
    } else {
        MmFreePageListHead = Group->NextGroup;
    }

    if (Group->NextGroup) {
        Group->NextGroup->PreviousGroup = Group->PreviousGroup;
    } else {
        MmFreePageListTail = Group->PreviousGroup;
    }

    return Group->GroupBase;
}
