/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>

typedef struct {
    RtDList ListHeader;
    uint64_t Pages;
} BigFreeHeader;

extern MiPageEntry *MiPageList;
extern char *MiPoolVirtualOffset;
extern RtDList MiPoolBigFreeListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a the specified amount of pages from the pool space. The pool lock is
 *assumed to have already been acquired.
 *
 * PARAMETERS:
 *     Pages - How many pages we need.
 *
 * RETURN VALUE:
 *     Virtual (mapped) pointer to the allocated space, or NULL if we failed to allocate it.
 *-----------------------------------------------------------------------------------------------*/
void *MiAllocatePoolPages(uint64_t Pages) {
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

        /* Mark the base of the allocation as such, everything else should have already been marked
         * properly by FreePoolPages. */
        uint64_t PhysicalAddress = HalpGetPhysicalAddress(Base);
        MiPageEntry *BaseEntry = &MI_PAGE_ENTRY(PhysicalAddress);
        BaseEntry->Flags = MI_PAGE_FLAGS_USED | MI_PAGE_FLAGS_POOL_BASE;
        BaseEntry->Pages = Pages;
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

        /* Mark the current page as part of the pool. */
        MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);
        if (Offset) {
            Entry->Flags |= MI_PAGE_FLAGS_POOL_ITEM;
        } else {
            Entry->Flags |= MI_PAGE_FLAGS_POOL_BASE;
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
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiFreePoolPages(void *Base) {
    uint64_t PhysicalAddress = HalpGetPhysicalAddress(Base);
    MiPageEntry *BaseEntry = &MI_PAGE_ENTRY(PhysicalAddress);
    uint64_t Pages = BaseEntry->Pages;
    if (!(BaseEntry->Flags & MI_PAGE_FLAGS_USED) || !(BaseEntry->Flags & MI_PAGE_FLAGS_POOL_BASE)) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, BaseEntry->Flags, 0, 0);
    }

    /* Validate all pages inside to make sure they are pool items; We probably should limit this to
     * debug builds? */
    BaseEntry->Flags = MI_PAGE_FLAGS_USED | MI_PAGE_FLAGS_POOL_ITEM;
    for (uint32_t Offset = MM_PAGE_SIZE; Offset < Pages << MM_PAGE_SHIFT; Offset += MM_PAGE_SIZE) {
        uint64_t PhysicalAdddress = HalpGetPhysicalAddress((char *)Base + Offset);
        MiPageEntry *ItemEntry = &MI_PAGE_ENTRY(PhysicalAdddress);
        if (!(ItemEntry->Flags & MI_PAGE_FLAGS_USED) ||
            !(ItemEntry->Flags & MI_PAGE_FLAGS_POOL_ITEM)) {
            KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, ItemEntry->Flags, 0, 0);
        }
    }

    /* TODO: Reduce fragmentation (deferred merge pages probably?). */
    BigFreeHeader *Header = Base;
    Header->Pages = Pages;
    RtAppendDList(&MiPoolBigFreeListHead, &Header->ListHeader);
}
