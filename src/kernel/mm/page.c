/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/ki.h>
#include <kernel/mi.h>
#include <kernel/mm.h>
#include <os/containing_record.h>
#include <rt/list.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static RtDList *LoaderDescriptors = NULL;

RtDList MiMemoryDescriptorListHead;
MiPageEntry *MiPageList = NULL;
RtSList MiFreePageListHead;
KeSpinLock MiPageListLock = {0};
uint64_t MiTotalManagedPages = 0;
uint64_t MiTotalUnmanagedPages = 0;
uint64_t MiTotalReservedPages = 0;
uint64_t MiTotalCachedPages = 0;
uint64_t MiTotalUsedPages = 0;
uint64_t MiTotalFreePages = 0;
uint64_t MiTotalBootPages = 0;
uint64_t MiTotalGraphicsPages = 0;
uint64_t MiTotalPtePages = 0;
uint64_t MiTotalPfnPages = 0;
uint64_t MiTotalPoolPages = 0;
static uint64_t MiPageCount = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function counts the pages in a managed descriptor that overlap the reserved low 64KiB.
 *
 * PARAMETERS:
 *     Entry - Memory descriptor to inspect.
 *
 * RETURN VALUE:
 *     Number of low pages in the descriptor.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t GetLowReservedPages(MiMemoryDescriptor *Entry) {
    if (Entry->BasePage >= 0x10) {
        return 0;
    }

    uint64_t EndPage = Entry->BasePage + Entry->PageCount;
    if (EndPage > 0x10) {
        EndPage = 0x10;
    }

    return EndPage - Entry->BasePage;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a given amount of contiguous pages directly from the osloader
 *     memory map; This should only be used before the initialization of the pool and the PFN.
 *
 * PARAMETERS:
 *     Pages - How many contiguous pages we need.
 *
 * RETURN VALUE:
 *     Physical address of the contigous pages, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t MiAllocateEarlyPages(uint32_t Pages) {
    if (!LoaderDescriptors) {
        return 0;
    }

    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->Type != MI_DESCR_FREE) {
            continue;
        }

        uint64_t EndPage = Entry->BasePage + Entry->PageCount;
        uint64_t BasePage = Entry->BasePage < 0x10 ? 0x10 : Entry->BasePage;
        if (BasePage > EndPage || Pages > EndPage - BasePage) {
            continue;
        }

        uint64_t Result = BasePage << MM_PAGE_SHIFT;
        Entry->BasePage = BasePage + Pages;
        Entry->PageCount = EndPage - Entry->BasePage;
        MiTotalUsedPages += Pages;
        MiTotalFreePages -= Pages;
        return Result;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function prepares for the memory manager initialization by setting up a really dummy
 *     page allocator that uses the osloader memory map directly. HalpMap(Non)ContiguousPages should
 *     automatically use this during early boot (as needed).
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializeEarlyPageAllocator(KiLoaderBlock *LoaderBlock) {
    LoaderDescriptors = LoaderBlock->Basic.MemoryDescriptorListHead;

    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

        /* Unmapping the 1:1 firware temp regions should be already okay to do. */
        if (Entry->Type == MI_DESCR_FIRMWARE_TEMPORARY ||
            Entry->Type == MI_DESCR_FIRMWARE_PERMANENT) {
            HalpUnmapPages(
                (void *)(Entry->BasePage << MM_PAGE_SHIFT), Entry->PageCount << MM_PAGE_SHIFT);
        }

        if (Entry->Type == MI_DESCR_SYSTEM_RESERVED) {
            MiTotalUnmanagedPages += Entry->PageCount;
            continue;
        }

        /* We just need to remember to make sure we won't use the low 64KiB; They are reserved if
         * the kernel needs any low memory (for initializing SMP or anything else like that). */
        uint64_t LowPages = GetLowReservedPages(Entry);
        uint64_t RemainingPages = Entry->PageCount - LowPages;
        MiTotalManagedPages += Entry->PageCount;
        MiTotalReservedPages += LowPages;

        switch (Entry->Type) {
            case MI_DESCR_FREE:
            case MI_DESCR_FIRMWARE_TEMPORARY:
                MiTotalFreePages += RemainingPages;
                break;
            case MI_DESCR_PAGE_MAP:
                MiTotalPtePages += RemainingPages;
                MiTotalUsedPages += RemainingPages;
                break;
            case MI_DESCR_LOADED_PROGRAM:
                MiTotalBootPages += RemainingPages;
                MiTotalUsedPages += RemainingPages;
                break;
            case MI_DESCR_GRAPHICS_BUFFER:
                MiTotalGraphicsPages += RemainingPages;
                MiTotalUsedPages += RemainingPages;
                break;
            case MI_DESCR_OSLOADER_TEMPORARY:
                MiTotalUsedPages += RemainingPages;
                break;
            case MI_DESCR_FIRMWARE_PERMANENT:
                MiTotalReservedPages += RemainingPages;
                break;
            default:
                KeFatalError(
                    KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
                    KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
                    Entry->Type,
                    Entry->BasePage,
                    Entry->PageCount);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves up all memory descriptors in kernel memory, and initializes the physical
 *     page allocator (and the page database).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePageAllocator(void) {
    /* The PFN database only tracks pages we might allocate, find the max addressable FREE
     * page. */
    uint64_t MaxAddressablePage = 0;
    uint64_t MemoryDescriptorListSize = 0;
    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

        /* Let's use the fact we're iterating through the list and already save its size (for
         * copying it into kernel land later). */
        MemoryDescriptorListSize += sizeof(MiMemoryDescriptor);

        if (Entry->Type <= MI_DESCR_FIRMWARE_PERMANENT) {
            uint64_t EndPage = Entry->BasePage + Entry->PageCount;
            if (EndPage > MaxAddressablePage) {
                MaxAddressablePage = EndPage;
            }
        }
    }

    /* Grab some physical memory and map it for the PFN database. This should be the last place we
     * need EarlyAllocatePages. */
    if (!MaxAddressablePage || MaxAddressablePage > UINT64_MAX / sizeof(MiPageEntry)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            2,
            MaxAddressablePage,
            0);
    }

    uint64_t Size = MaxAddressablePage * sizeof(MiPageEntry);
    if (Size > UINT64_MAX - (MM_PAGE_SIZE - 1)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            3,
            Size,
            0);
    }

    uint64_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    uint64_t PhysicalAddress = MiAllocateEarlyPages(Pages);
    if (!PhysicalAddress) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    if (!HalpMapContiguousPages(
            (void *)MI_PFN_START, PhysicalAddress, Pages << MM_PAGE_SHIFT, MI_MAP_WRITE)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    MiPageList = (void *)MI_PFN_START;
    MiPageCount = MaxAddressablePage;
    MiTotalPfnPages = Pages;

    /* Setup the page allocator (marking the free pages as free). */
    memset(MiPageList, 0, Size);
    for (uint64_t Page = 0; Page < MaxAddressablePage; Page++) {
        MiPageList[Page].Used = 1;
    }

    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->Type > MI_DESCR_FIRMWARE_PERMANENT) {
            continue;
        }

        if (Entry->Type != MI_DESCR_FREE && Entry->Type != MI_DESCR_FIRMWARE_TEMPORARY) {
            continue;
        }

        uint64_t StartPage = Entry->BasePage < 0x10 ? 0x10 : Entry->BasePage;
        uint64_t EndPage = Entry->BasePage + Entry->PageCount;
        for (uint64_t Page = StartPage; Page < EndPage; Page++) {
            MiPageList[Page].Used = 0;
            RtPushSList(&MiFreePageListHead, &MiPageList[Page].ListHeader);
        }
    }

    /* We're forced to initialize the pool trackers before continuing (or we'll crash when trying to
     * account for the allocation because MiPoolTracker will be a NULL pointer). */
    MiInitializePoolTracker();

    /* Now we should be free to allocate some pool memory and copy the memory descriptor list in its
     * current state. */
    MiMemoryDescriptor *Descriptor = MmAllocatePool(MemoryDescriptorListSize, MM_POOL_TAG_PFN);
    if (!Descriptor) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    RtInitializeDList(&MiMemoryDescriptorListHead);
    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        memcpy(Descriptor, Entry, sizeof(MiMemoryDescriptor));
        RtAppendDList(&MiMemoryDescriptorListHead, &Descriptor->ListHeader);
        Descriptor++;
    }

    /* Marking the pointer as NULL should disable EarlyAllocatePages. */
    LoaderDescriptors = NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function wraps up the memory manager initialization by freeing and unmapping the
 *     OSLOADER/1-to-1 mapping regions. This should only be called after LoaderBlock (and anything
 *     else from OSLOADER) has already been used and saved somewhere else.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiReleaseBootRegions(void) {
    for (RtDList *ListHeader = MiMemoryDescriptorListHead.Next;
         ListHeader != &MiMemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->Type != MI_DESCR_OSLOADER_TEMPORARY) {
            continue;
        }

        uint64_t StartPage = Entry->BasePage < 0x10 ? 0x10 : Entry->BasePage;
        uint64_t EndPage = Entry->BasePage + Entry->PageCount;
        for (uint64_t Page = StartPage; Page < EndPage; Page++) {
            MiPageEntry *PageEntry = &MiPageList[Page];
            if (!PageEntry->Used || PageEntry->PoolItem || PageEntry->PoolBase) {
                KeFatalError(
                    KE_PANIC_BAD_PFN_HEADER, Page << MM_PAGE_SHIFT, PageEntry->Flags, 0, 0);
            }

            PageEntry->Used = 0;
            RtPushSList(&MiFreePageListHead, &PageEntry->ListHeader);
        }

        uint64_t ReleasedPages = EndPage - StartPage;
        MiTotalUsedPages -= ReleasedPages;
        MiTotalFreePages += ReleasedPages;

        HalpUnmapPages(
            (void *)(Entry->BasePage << MM_PAGE_SHIFT), Entry->PageCount << MM_PAGE_SHIFT);
    }
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
uint64_t MmAllocateSinglePage() {
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    KeProcessor *Processor = KeGetCurrentProcessor();

    /* Trigger a cache refill if it's the first allocation we're doing (or if we dropped below the
     * lower limit). */
    if (Processor->FreePageListSize < MI_PROCESSOR_PAGE_CACHE_MIN_SIZE) {
        KeAcquireSpinLockAtCurrentIrql(&MiPageListLock);

        for (int i = 0; i < MI_PROCESSOR_PAGE_CACHE_BATCH_SIZE; i++) {
            RtSList *ListHeader = RtPopSList(&MiFreePageListHead);
            if (!ListHeader) {
                break;
            }

            /* The main allocation path is expected to check for the validity of the pages it pops,
             * so we just add them to the list here. */
            RtPushSList(&Processor->FreePageListHead, ListHeader);
            Processor->FreePageListSize++;
            __atomic_add_fetch(&MiTotalCachedPages, 1, __ATOMIC_RELAXED);
            __atomic_sub_fetch(&MiTotalFreePages, 1, __ATOMIC_RELAXED);
        }

        KeReleaseSpinLockAtCurrentIrql(&MiPageListLock);
    }

    /* Now we should just be able to pop from the local cache (if that fails, the system is out of
     * memory). */
    RtSList *ListHeader = RtPopSList(&Processor->FreePageListHead);
    if (!ListHeader) {
        KeLowerIrql(OldIrql);
        return 0;
    }

    Processor->FreePageListSize--;
    KeLowerIrql(OldIrql);
    __atomic_sub_fetch(&MiTotalCachedPages, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&MiTotalUsedPages, 1, __ATOMIC_RELAXED);

    /* Make sure the flags make sense (if not, we probably have a corrupted PFN free list). */
    MiPageEntry *Entry = CONTAINING_RECORD(ListHeader, MiPageEntry, ListHeader);
    if (Entry->Used || Entry->PoolItem) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER, MI_PAGE_BASE(Entry), Entry->Flags, 0, 0);
    }

    Entry->Used = 1;
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
void MmFreeSinglePage(uint64_t PhysicalAddress) {
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    KeProcessor *Processor = KeGetCurrentProcessor();

    /* Use MmFreePool to free big pool allocations, instead of us! */
    MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);
    if (!Entry->Used || Entry->PoolItem) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER, PhysicalAddress, Entry->Flags, 0, 0);
    }

    /* Update all stat, and check if we can just append this to the local cache. */
    Entry->Used = 0;
    if (Processor->FreePageListSize < MI_PROCESSOR_PAGE_CACHE_MAX_SIZE) {
        RtPushSList(&Processor->FreePageListHead, &Entry->ListHeader);
        Processor->FreePageListSize++;
        KeLowerIrql(OldIrql);
        __atomic_add_fetch(&MiTotalCachedPages, 1, __ATOMIC_RELAXED);
        __atomic_sub_fetch(&MiTotalUsedPages, 1, __ATOMIC_RELAXED);
        return;
    }

    /* Otherwise, append the page to the global free page list (we do need the global lock for
     * this). */
    KeAcquireSpinLockAtCurrentIrql(&MiPageListLock);
    RtPushSList(&MiFreePageListHead, &Entry->ListHeader);
    KeReleaseSpinLockAndLowerIrql(&MiPageListLock, OldIrql);
    __atomic_sub_fetch(&MiTotalUsedPages, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&MiTotalFreePages, 1, __ATOMIC_RELAXED);
}
