/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>

extern char *MiPoolVirtualOffset;

static RtSList FreeLists[4] = {};
static KeSpinLock Lock = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a the specified amount of pages from the pool space.
 *
 * PARAMETERS:
 *     Pages - How many pages we need.
 *
 * RETURN VALUE:
 *     Virtual (mapped) pointer to the allocated space, or NULL if we failed to allocate it.
 *-----------------------------------------------------------------------------------------------*/
void *MiAllocatePoolPages(uint32_t Pages) {
    if (!Pages) {
        return NULL;
    }

    /* For smaller allocations (up to 4 pages, which should be more common than other big
     * allocations), we have a special path (caching of recently freed entries). */
    if (Pages <= 4) {
        /* Start by checking in the per-processor list (as that's lock-free). */
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
        KeProcessor *Processor = KeGetCurrentProcessor();
        RtSList *ListHeader = RtPopSList(&Processor->FreePoolPageListHead[Pages - 1]);

        /* And if that fails, try grabbing something out of the global list (that needs a lock). */
        if (ListHeader) {
            Processor->FreePoolPageListSize[Pages - 1]--;
            KeLowerIrql(OldIrql);
        } else {
            KeAcquireSpinLockAtCurrentIrql(&Lock);
            ListHeader = RtPopSList(&FreeLists[Pages - 1]);
            KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
        }

        if (ListHeader) {
            uint64_t PhysicalAddress = HalpGetPhysicalAddress(ListHeader);
            MiPageEntry *BaseEntry = &MI_PAGE_ENTRY(PhysicalAddress);

            if (!BaseEntry->PoolBase || BaseEntry->Pages != Pages) {
                KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, BaseEntry->Flags, 0, 0);
            }

            return ListHeader;
        }
    }

    /* This might end up happening on systems with a lower virtual space reservation (so, 32-bit
     * systems), so, TODO: change this to a balanced tree of some kind. */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Lock, KE_IRQL_DISPATCH);
    if (MiPoolVirtualOffset + (Pages << MM_PAGE_SHIFT) > (char *)MI_POOL_END) {
        KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
        return NULL;
    }

    char *VirtualAddress = MiPoolVirtualOffset;
    MiPoolVirtualOffset += Pages << MM_PAGE_SHIFT;
    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);

    for (uint64_t Offset = 0; Offset < Pages << MM_PAGE_SHIFT; Offset += MM_PAGE_SIZE) {
        /* TODO: What exactly should be do when we fail to map/allocate the space, unmap+free
         * the pages, mark them as big pool free? */
        uint64_t PhysicalAddress = MmAllocateSinglePage();
        if (!PhysicalAddress) {
            return NULL;
        } else if (!HalpMapContiguousPages(
                       VirtualAddress + Offset, PhysicalAddress, MM_PAGE_SIZE, MI_MAP_WRITE)) {
            return NULL;
        }

        /* Mark the pages of the pool as such. */
        MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);
        Entry->Used = 1;
        Entry->PoolItem = 1;
        if (!Offset) {
            Entry->PoolBase = 1;
            Entry->Pages = Pages;
        }
    }

    __atomic_fetch_add(&MiTotalPoolPages, Pages, __ATOMIC_RELAXED);
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
    MiPageEntry *PageEntry = &MI_PAGE_ENTRY(PhysicalAddress);
    uint32_t Pages = PageEntry->Pages;
    if (!PageEntry->Used || !PageEntry->PoolBase) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, PageEntry->Flags, 0, 0);
    }

    /* For small (up to 4 pages) blocks, cache this entry as is in their respective buckets (rather
     * than returning the memory). TODO: Should we limit the size of the global cache? Or leave it
     * uncapped, and return the memory (or part of it) if our memory usage is above a certain
     * point? */
    if (Pages <= 4) {
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
        KeProcessor *Processor = KeGetCurrentProcessor();

        if (Processor->FreePoolPageListSize[Pages - 1] < MI_PROCESSOR_POOL_CACHE_MAX_SIZE) {
            RtPushSList(&Processor->FreePoolPageListHead[Pages - 1], Base);
            Processor->FreePoolPageListSize[Pages - 1]++;
            KeLowerIrql(OldIrql);
        } else {
            KeAcquireSpinLockAtCurrentIrql(&Lock);
            RtPushSList(&FreeLists[Pages - 1], Base);
            KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
        }

        return Pages;
    }

    /* Otherwise, start by freeing the base/first block (and unmapping it). */
    PageEntry->PoolBase = 0;
    MmFreeSinglePage(PhysicalAddress);

    /* And follow up by validating and freeing up the remaining memory. */
    for (uint32_t i = 1; i < Pages; i++) {
        PhysicalAddress = HalpGetPhysicalAddress((char *)Base + (i << MM_PAGE_SHIFT));
        PageEntry = &MI_PAGE_ENTRY(PhysicalAddress);
        if (!PageEntry->Used || !PageEntry->PoolItem) {
            KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, PageEntry->Flags, 0, 0);
        }

        PageEntry->PoolItem = 0;
        MmFreeSinglePage(PhysicalAddress);
    }

    /* And wrap up by unmapping the whole range; This is going to leak virtual space left and right
     * if we do it too much, so, maybe, TODO: fix this? Shouldn't be a problem on 64-bit address
     * spaces, but it can be problematic on 32-bit systems. */
    HalpUnmapPages(Base, Pages << MM_PAGE_SHIFT);
    __atomic_fetch_sub(&MiTotalPoolPages, Pages, __ATOMIC_RELAXED);
    return Pages;
}
