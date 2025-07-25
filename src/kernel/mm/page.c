/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <string.h>

static RtDList *LoaderDescriptors = NULL;

RtDList MiMemoryDescriptorListHead;
MiPageEntry *MiPageList = NULL;
RtSList MiFreePageListHead;
KeSpinLock MiPageListLock = {0};
uint64_t MiTotalSystemPages = 0;
uint64_t MiTotalReservedPages = 0;
uint64_t MiTotalCachedPages = 0;
uint64_t MiTotalUsedPages = 0;
uint64_t MiTotalFreePages = 0;
uint64_t MiTotalBootPages = 0;
uint64_t MiTotalGraphicsPages = 0;
uint64_t MiTotalPtePages = 0;
uint64_t MiTotalPfnPages = 0;
uint64_t MiTotalPoolPages = 0;

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
        if (Entry->PageCount < Pages || Entry->Type != MI_DESCR_FREE) {
            continue;
        }

        uint64_t Result = Entry->BasePage << MM_PAGE_SHIFT;
        Entry->BasePage += Pages;
        Entry->PageCount -= Pages;
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
    LoaderDescriptors = LoaderBlock->MemoryDescriptorListHead;

    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

        /* Unmapping the 1:1 firware temp regions should be already okay to do. */
        if (Entry->Type == MI_DESCR_FIRMWARE_TEMPORARY ||
            Entry->Type == MI_DESCR_FIRMWARE_PERMANENT) {
            HalpUnmapPages(
                (void *)(Entry->BasePage << MM_PAGE_SHIFT), Entry->PageCount << MM_PAGE_SHIFT);
        }

        /* We need to make sure we won't use the low 64KiB; They are reserved if the kernel needs
         * any low memory (for initializing SMP or anything else like that). */
        if (Entry->BasePage < 0x10) {
            uint64_t Pages = 0x10 - Entry->BasePage;
            if (Entry->PageCount < Pages) {
                Pages = Entry->PageCount;
            }

            MiTotalReservedPages += Pages;
            Entry->PageCount -= Pages;
            Entry->BasePage = 0x10;
        }

        /* Otherwise, just update the global page stats using the data we from this region. */
        if (Entry->Type >= MI_DESCR_FIRMWARE_PERMANENT) {
            MiTotalReservedPages += Entry->PageCount;
        } else if (Entry->Type == MI_DESCR_GRAPHICS_BUFFER) {
            MiTotalGraphicsPages += Entry->PageCount;
            MiTotalUsedPages += Entry->PageCount;
        } else if (Entry->Type == MI_DESCR_PAGE_MAP || Entry->Type == MI_DESCR_LOADED_PROGRAM) {
            MiTotalBootPages += Entry->PageCount;
            MiTotalUsedPages += Entry->PageCount;
        } else {
            MiTotalFreePages += Entry->PageCount;
        }
    }

    /* Now calculate the total amount of pages the system has. */
    MiTotalSystemPages = MiTotalReservedPages + MiTotalUsedPages + MiTotalFreePages;
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
            MaxAddressablePage = Entry->BasePage + Entry->PageCount;
        }
    }

    /* Grab some physical memory and map it for the PFN database. This should be the last place we
     * need EarlyAllocatePages. */
    uint64_t Size = MaxAddressablePage * sizeof(MiPageEntry);
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

    void *PageListBase = (void *)(MI_VIRTUAL_OFFSET + PhysicalAddress);
    if (!HalpMapContiguousPages(
            PageListBase, PhysicalAddress, Pages << MM_PAGE_SHIFT, MI_MAP_WRITE)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    MiPageList = PageListBase;
    MiTotalPfnPages = Pages;

    /* Setup the page allocator (marking the free pages as free). */
    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->Type > MI_DESCR_FIRMWARE_PERMANENT) {
            continue;
        }

        MiPageEntry *Group = &MiPageList[Entry->BasePage];

        if (Entry->Type != MI_DESCR_FREE && Entry->Type != MI_DESCR_FIRMWARE_TEMPORARY) {
            for (uint32_t i = 0; i < Entry->PageCount; i++) {
                Group[i].Used = 1;
                Group[i].PoolItem = 0;
                Group[i].PoolBase = 0;
            }
        } else {
            for (uint32_t i = 0; i < Entry->PageCount; i++) {
                Group[i].Used = 0;
                Group[i].PoolItem = 0;
                Group[i].PoolBase = 0;
                RtPushSList(&MiFreePageListHead, &Group[i].ListHeader);
            }
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

        MiPageEntry *Group = &MiPageList[Entry->BasePage];
        for (uint64_t i = 0; i < (uint64_t)Entry->PageCount; i++) {
            Group[i].Used = 0;
            Group[i].PoolItem = 0;
            Group[i].PoolBase = 0;
            RtPushSList(&MiFreePageListHead, &Group[i].ListHeader);
        }

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
    } else {
        Processor->FreePageListSize--;
        KeLowerIrql(OldIrql);
        __atomic_sub_fetch(&MiTotalCachedPages, 1, __ATOMIC_RELAXED);
        __atomic_add_fetch(&MiTotalUsedPages, 1, __ATOMIC_RELAXED);
    }

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
