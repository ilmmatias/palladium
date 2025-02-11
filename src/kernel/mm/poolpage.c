/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>

typedef struct {
    RtDList ListHeader;
    uint64_t Pages;
} BigFreeHeader;

extern char *MiPoolVirtualOffset;
extern RtDList MiPoolBigFreeListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a the specified amount of pages from the pool space. The pool lock
 *     is assumed to have already been acquired.
 *
 * PARAMETERS:
 *     Pages - How many pages we need.
 *
 * RETURN VALUE:
 *     Virtual (mapped) pointer to the allocated space, or NULL if we failed to allocate it.
 *-----------------------------------------------------------------------------------------------*/
void *MiAllocatePoolPages(uint32_t Pages) {
    for (RtDList *ListHeader = MiPoolBigFreeListHead.Next; ListHeader != &MiPoolBigFreeListHead;
         ListHeader = ListHeader->Next) {
        BigFreeHeader *Header = CONTAINING_RECORD(ListHeader, BigFreeHeader, ListHeader);
        if (Header->Pages < Pages) {
            continue;
        }

        /* We can carve out of the free entry, just make sure to remove from the list if we reached
         * no pages left. */
        void *Base = Header;
        if (Header->Pages == Pages) {
            RtUnlinkDList(&Header->ListHeader);
        } else {
            Header->Pages -= Pages;
            Base = (char *)Header + (Header->Pages << MM_PAGE_SHIFT);
        }

        /* And we're done here, just mark ourselves as the base of the allocation. */
        uint64_t PhysicalAddress = HalpGetPhysicalAddress(Base);
        MiPageEntry *BaseEntry = &MI_PAGE_ENTRY(PhysicalAddress);
        BaseEntry->PoolBase = 1;
        BaseEntry->Pages = Pages;
        MiTotalUsedPages += Pages;
        return Base;
    }

    /* Otherwise, carve more pool space out. */
    char *VirtualAddress = MiPoolVirtualOffset;
    MiPoolVirtualOffset += Pages << MM_PAGE_SHIFT;

    for (uint64_t Offset = 0; Offset < Pages << MM_PAGE_SHIFT; Offset += MM_PAGE_SIZE) {
        /* TODO: What exactly should be do when we fail to map/allocate the space, unmap+free the
         * pages, mark them as big pool free? */
        uint64_t PhysicalAddress = MmAllocateSinglePage();
        if (!PhysicalAddress) {
            return NULL;
        } else if (!HalpMapPages(
                       VirtualAddress + Offset, PhysicalAddress, MM_PAGE_SIZE, MI_MAP_WRITE)) {
            return NULL;
        }

        /* Mark the pages of the pool as such. */
        MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);
        MiTotalPoolPages += 1;
        Entry->Used = 1;
        Entry->PoolItem = 1;
        if (!Offset) {
            Entry->PoolBase = 1;
            Entry->Pages = Pages;
        }
    }

    return VirtualAddress;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns all pages belonging to the given allocation into the free list.
 *
 * PARAMETERS:
 *     Base - First virtual address of the allocation.
 *
 * RETURN VALUE:
 *     How many pages the allocation had.
 *-----------------------------------------------------------------------------------------------*/
uint32_t MiFreePoolPages(void *Base) {
    uint64_t PhysicalAddress = HalpGetPhysicalAddress(Base);
    MiPageEntry *BaseEntry = &MI_PAGE_ENTRY(PhysicalAddress);
    uint32_t Pages = BaseEntry->Pages;
    if (!BaseEntry->Used || !BaseEntry->PoolBase) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, BaseEntry->Flags, 0, 0);
    }

    /* With this, the current state of all pages in this allocation should be:
     * Used=1, PoolItem=1, PoolBase=0. */
    BaseEntry->PoolBase = 0;
    MiTotalUsedPages -= Pages;

    /* TODO: Reduce fragmentation (merge pages probably, either now, or on the idle thread). */
    BigFreeHeader *Header = Base;
    Header->Pages = Pages;
    RtAppendDList(&MiPoolBigFreeListHead, &Header->ListHeader);
    return Pages;
}
