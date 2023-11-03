/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <mi.h>
#include <stddef.h>

MiPageEntry *MiPageList = NULL;
MiPageEntry *MiFreePageListHead = NULL;
MiPageEntry *MiFreePageListTail = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tells the memory manager we'll use the specified physical memory pages.
 *
 * PARAMETERS:
 *     PageList - Array with the page addresses.
 *     Pages - How many pages the list has.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MmReferencePages(uint64_t *PageList, uint32_t Pages) {
    for (uint32_t i = 0; i < Pages; i++) {
        MiPageList[PageList[i] >> MM_PAGE_SHIFT].References++;
    }
}

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
    if (!Pages) {
        Pages = 1;
    }

    MiPageEntry *Group = MiFreePageListHead;
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

        for (uint32_t i = 0; i < Pages; i++) {
            MiPageList[(Base >> MM_PAGE_SHIFT) + i].References = 1;
        }

        return Base;
    }

    /* On perfect match, we have two options:
           - Set the size to zero, and leave it in place, waiting for clean up later.
           - Remove the group from the linked list; This is what we're doing below. */

    if (Group->PreviousGroup) {
        Group->PreviousGroup->NextGroup = Group->NextGroup;
    } else {
        MiFreePageListHead = Group->NextGroup;
    }

    if (Group->NextGroup) {
        Group->NextGroup->PreviousGroup = Group->PreviousGroup;
    } else {
        MiFreePageListTail = Group->PreviousGroup;
    }

    for (uint32_t i = 0; i < Pages; i++) {
        MiPageList[(Group->GroupBase >> MM_PAGE_SHIFT) + i].References = 1;
    }

    return Group->GroupBase;
}
