/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <rt/bitmap.h>

extern RtBitmap MiPoolBitmap;
extern uint64_t MiPoolBitmapHint;

static RtSList FreeLists[4] = {};
static KeSpinLock FreeListLock[4] = {0};
static KeSpinLock BitmapLock = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a the specified amount of pages from the pool space. We expect to be
 *     called at DISPATCH IRQL.
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
        KeProcessor *Processor = KeGetCurrentProcessor();
        RtSList *ListHeader = RtPopSList(&Processor->FreePoolPageListHead[Pages - 1]);

        /* And if that fails, try grabbing something out of the global list (that needs a lock). */
        if (ListHeader) {
            Processor->FreePoolPageListSize[Pages - 1]--;
        } else {
            KeAcquireSpinLockAtCurrentIrql(&FreeListLock[Pages - 1]);
            ListHeader = RtPopSList(&FreeLists[Pages - 1]);
            KeReleaseSpinLockAtCurrentIrql(&FreeListLock[Pages - 1]);
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

    /* Otherwise, we need to grab more virtual space (this needs the bitmap lock). */
    KeAcquireSpinLockAtCurrentIrql(&BitmapLock);
    uint64_t Index = RtFindClearBitsAndSet(&MiPoolBitmap, MiPoolBitmapHint, Pages);
    if (Index == (uint64_t)-1) {
        KeReleaseSpinLockAtCurrentIrql(&BitmapLock);
        return NULL;
    } else {
        MiPoolBitmapHint = Index + Pages;
        KeReleaseSpinLockAtCurrentIrql(&BitmapLock);
    }

    char *VirtualAddress = (char *)(MI_POOL_START + (Index << MM_PAGE_SHIFT));
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
 *     This function returns all pages belonging to the given allocation into the free list. We
 *     expect to be called at DISPATCH IRQL.
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
        KeProcessor *Processor = KeGetCurrentProcessor();

        if (Processor->FreePoolPageListSize[Pages - 1] < MI_PROCESSOR_POOL_CACHE_MAX_SIZE) {
            RtPushSList(&Processor->FreePoolPageListHead[Pages - 1], Base);
            Processor->FreePoolPageListSize[Pages - 1]++;
        } else {
            KeAcquireSpinLockAtCurrentIrql(&FreeListLock[Pages - 1]);
            RtPushSList(&FreeLists[Pages - 1], Base);
            KeReleaseSpinLockAtCurrentIrql(&FreeListLock[Pages - 1]);
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

    /* And wrap up by unmapping and returning the whole range (the later one needs to be done under
     * the lock). */
    HalpUnmapPages(Base, Pages << MM_PAGE_SHIFT);

    uint64_t Index = ((uint64_t)Base - MI_POOL_START) >> MM_PAGE_SHIFT;
    KeAcquireSpinLockAtCurrentIrql(&BitmapLock);
    RtClearBits(&MiPoolBitmap, Index, Pages);
    MiPoolBitmapHint = Index;
    KeReleaseSpinLockAtCurrentIrql(&BitmapLock);

    __atomic_fetch_sub(&MiTotalPoolPages, Pages, __ATOMIC_RELAXED);
    return Pages;
}
