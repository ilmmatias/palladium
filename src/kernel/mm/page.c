/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kd.h>
#include <kernel/ke.h>
#include <kernel/ki.h>
#include <kernel/mi.h>
#include <kernel/mm.h>
#include <os/containing_record.h>
#include <os/intrin.h>
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
static uint64_t MiEarlyRollbackPages[MI_EARLY_ROLLBACK_PAGE_COUNT];
static uint32_t MiEarlyRollbackPageCount = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates the loader's memory descriptor list before the memory manager uses
 *     it for accounting or PFN sizing.
 *
 * PARAMETERS:
 *     ListHead - Loader memory descriptor list head.
 *
 * RETURN VALUE:
 *     true if the list is linked, sorted, disjoint, nonempty, and contains only known types.
 *-----------------------------------------------------------------------------------------------*/
static bool ValidateMemoryDescriptors(RtDList *ListHead) {
    if (!ListHead || !ListHead->Next || !ListHead->Prev) {
        return false;
    }

    /* Detect a malformed cycle which does not return to the list head without imposing an
       artificial descriptor-count limit on fragmented firmware maps. */
    RtDList *Slow = ListHead->Next;
    RtDList *Fast = ListHead->Next;
    while (Fast != ListHead) {
        if (!Fast || !Fast->Next) {
            return false;
        }
        Fast = Fast->Next;
        if (Fast == ListHead) {
            break;
        }
        if (!Fast->Next || !Slow || !Slow->Next) {
            return false;
        }

        Fast = Fast->Next;
        Slow = Slow->Next;
        if (Fast == Slow) {
            return false;
        }
    }

    uint64_t PreviousEnd = 0;
    uint64_t DescriptorCount = 0;
    for (RtDList *ListHeader = ListHead->Next; ListHeader != ListHead;
         ListHeader = ListHeader->Next) {
        if (!ListHeader || !ListHeader->Next || !ListHeader->Prev ||
            ListHeader->Next->Prev != ListHeader || ListHeader->Prev->Next != ListHeader) {
            return false;
        }
        DescriptorCount++;

        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (!Entry->PageCount || Entry->Type > MI_DESCR_SYSTEM_RESERVED ||
            Entry->BasePage > UINT64_MAX - Entry->PageCount) {
            return false;
        }

        if (DescriptorCount != 1 && Entry->BasePage < PreviousEnd) {
            return false;
        }

        PreviousEnd = Entry->BasePage + Entry->PageCount;
    }

    return DescriptorCount != 0;
}

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
    if (Pages == 1 && MiEarlyRollbackPageCount) {
        uint64_t Result = MiEarlyRollbackPages[--MiEarlyRollbackPageCount];
        MiTotalUsedPages++;
        MiTotalFreePages--;
        return Result;
    }

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
 *     This function returns one page allocated by the early allocator after a failed page-table
 *     transaction. The PFN allocator consumes any unreused entries when it initializes.
 *
 * PARAMETERS:
 *     PhysicalAddress - Page-aligned physical address to return.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiReleaseEarlyPage(uint64_t PhysicalAddress) {
    if (MiPageList || (PhysicalAddress & (MM_PAGE_SIZE - 1)) ||
        MiEarlyRollbackPageCount == MI_EARLY_ROLLBACK_PAGE_COUNT) {
        KeFatalError(
            KE_PANIC_BAD_PFN_HEADER,
            PhysicalAddress,
            MiEarlyRollbackPageCount,
            MI_EARLY_ROLLBACK_PAGE_COUNT,
            0);
    }

    MiEarlyRollbackPages[MiEarlyRollbackPageCount++] = PhysicalAddress;
    MiTotalUsedPages--;
    MiTotalFreePages++;
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
    if (!ValidateMemoryDescriptors(LoaderDescriptors)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            1,
            0,
            0);
    }

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

        MiTotalManagedPages += Entry->PageCount;
        uint64_t LowPages = GetLowReservedPages(Entry);
        uint64_t RemainingPages = Entry->PageCount - LowPages;
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

    /* Start with every addressable PFN explicitly unavailable. Descriptors then opt allocatable
     * pages into the free list; early allocations removed from free descriptors remain used. */
    memset(MiPageList, 0, Size);
    for (uint64_t Page = 0; Page < MaxAddressablePage; Page++) {
        MiPageList[Page].Used = 1;
    }

    /* Setup the page allocator (marking the allocatable pages as free). */
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

    /* Early page-table rollback pages were carved out of loader descriptors. Any page which was
       not reused before PFN initialization must be returned explicitly to the global free list. */
    for (uint32_t Index = 0; Index < MiEarlyRollbackPageCount; Index++) {
        uint64_t Page = MiEarlyRollbackPages[Index] >> MM_PAGE_SHIFT;
        if (Page >= MiPageCount || !MiPageList[Page].Used) {
            KeFatalError(
                KE_PANIC_BAD_PFN_HEADER,
                MiEarlyRollbackPages[Index],
                Page < MiPageCount ? MiPageList[Page].Flags : UINT64_MAX,
                Index,
                0);
        }

        MiPageList[Page].Used = 0;
        RtPushSList(&MiFreePageListHead, &MiPageList[Page].ListHeader);
    }
    MiEarlyRollbackPageCount = 0;

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

#ifdef PALLADIUM_ENABLE_SELF_TESTS
typedef struct {
    uint8_t *SeenPages;
    uint32_t OwnerProcessor;
    volatile uint32_t ReadyProcessors;
    volatile bool ReleaseProcessors;
    bool Valid;
    uint32_t FailureCode;
    uint64_t FailureValue1;
    uint64_t FailureValue2;
} MiVerifyContext;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks one PFN as present in exactly one allocator free list.
 *
 * PARAMETERS:
 *     Context - Active verifier context.
 *     ListHeader - Intrusive free-list entry to validate.
 *
 * RETURN VALUE:
 *     true if the entry is a unique, valid, free PFN.
 *-----------------------------------------------------------------------------------------------*/
static bool MarkFreePage(MiVerifyContext *Context, RtSList *ListHeader) {
    MiPageEntry *Entry = CONTAINING_RECORD(ListHeader, MiPageEntry, ListHeader);
    uintptr_t Address = (uintptr_t)Entry;
    uintptr_t Start = (uintptr_t)MiPageList;
    uintptr_t End = Start + MiPageCount * sizeof(MiPageEntry);
    if (Address < Start || Address >= End || (Address - Start) % sizeof(MiPageEntry)) {
        Context->FailureCode = 1;
        Context->FailureValue1 = Address;
        return false;
    }

    uint64_t Page = (Address - Start) / sizeof(MiPageEntry);
    uint8_t Mask = 1u << (Page & 7);
    if (Context->SeenPages[Page >> 3] & Mask || Entry->Used || Entry->PoolItem || Entry->PoolBase) {
        Context->FailureCode = 2;
        Context->FailureValue1 = Page;
        Context->FailureValue2 = Entry->Flags;
        return false;
    }

    Context->SeenPages[Page >> 3] |= Mask;
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function verifies the global and per-processor PFN free lists while every processor is
 *     held at IPI level. It never acquires a lower-IRQL lock.
 *
 * PARAMETERS:
 *     Context - Active verifier context.
 *
 * RETURN VALUE:
 *     true if list membership, PFN flags, cache sizes, and counters agree.
 *-----------------------------------------------------------------------------------------------*/
static bool VerifyPageLists(MiVerifyContext *Context) {
    uint64_t GlobalFreePages = 0;
    for (RtSList *ListHeader = MiFreePageListHead.Next; ListHeader; ListHeader = ListHeader->Next) {
        if (++GlobalFreePages > MiPageCount || !MarkFreePage(Context, ListHeader)) {
            return false;
        }
    }

    uint64_t CachedPages = 0;
    for (uint32_t ProcessorIndex = 0; ProcessorIndex < HalpOnlineProcessorCount; ProcessorIndex++) {
        KeProcessor *Processor = HalpProcessorList[ProcessorIndex];
        uint64_t ProcessorPages = 0;
        for (RtSList *ListHeader = Processor->FreePageListHead.Next; ListHeader;
             ListHeader = ListHeader->Next) {
            if (++ProcessorPages > MiPageCount || !MarkFreePage(Context, ListHeader)) {
                return false;
            }
        }

        if (ProcessorPages != Processor->FreePageListSize) {
            Context->FailureCode = 3;
            Context->FailureValue1 = ProcessorPages;
            Context->FailureValue2 = Processor->FreePageListSize;
            return false;
        }
        CachedPages += ProcessorPages;
    }

    if (GlobalFreePages != MiTotalFreePages || CachedPages != MiTotalCachedPages) {
        Context->FailureCode = 4;
        Context->FailureValue1 = GlobalFreePages;
        Context->FailureValue2 = MiTotalFreePages;
        return false;
    }

    for (uint64_t Page = 0; Page < MiPageCount; Page++) {
        MiPageEntry *Entry = &MiPageList[Page];
        bool Listed = (Context->SeenPages[Page >> 3] & (1u << (Page & 7))) != 0;
        if ((!Entry->Used) != Listed || (Entry->PoolItem && !Entry->Used) ||
            (Entry->PoolBase && (!Entry->PoolItem || !Entry->Pages))) {
            Context->FailureCode = 5;
            Context->FailureValue1 = Page;
            Context->FailureValue2 = Entry->Flags;
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function rendezvouses all processors for an allocation-free PFN list snapshot.
 *
 * PARAMETERS:
 *     ContextPointer - Active verifier context.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void VerifyPageIpi(void *ContextPointer) {
    MiVerifyContext *Context = ContextPointer;
    uint32_t Processor = KeGetCurrentProcessor()->Number;
    __atomic_add_fetch(&Context->ReadyProcessors, 1, __ATOMIC_ACQ_REL);

    if (Processor != Context->OwnerProcessor) {
        while (!__atomic_load_n(&Context->ReleaseProcessors, __ATOMIC_ACQUIRE)) {
            PauseProcessor();
        }
        return;
    }

    while (__atomic_load_n(&Context->ReadyProcessors, __ATOMIC_ACQUIRE) !=
           HalpOnlineProcessorCount) {
        PauseProcessor();
    }

    Context->Valid = VerifyPageLists(Context);
    __atomic_store_n(&Context->ReleaseProcessors, true, __ATOMIC_RELEASE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function verifies conservation and PFN/free-list consistency at an SMP quiescent point.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if every managed page belongs to exactly one top-level accounting state.
 *-----------------------------------------------------------------------------------------------*/
bool MiVerifyPageAccounting(void) {
    uint64_t AccountedPages = MiTotalReservedPages;
    if (AccountedPages > UINT64_MAX - MiTotalUsedPages) {
        return false;
    }

    AccountedPages += MiTotalUsedPages;
    if (AccountedPages > UINT64_MAX - MiTotalCachedPages) {
        return false;
    }

    AccountedPages += MiTotalCachedPages;
    if (AccountedPages > UINT64_MAX - MiTotalFreePages) {
        return false;
    }

    AccountedPages += MiTotalFreePages;
    if (AccountedPages != MiTotalManagedPages || MiPageCount > UINT64_MAX - 7) {
        return false;
    }

    size_t BitmapSize = (MiPageCount + 7) >> 3;
    uint8_t *SeenPages = MmAllocatePool(BitmapSize, MM_POOL_TAG_PFN);
    if (!SeenPages) {
        return false;
    }

    MiVerifyContext Context = {
        .SeenPages = SeenPages,
        .OwnerProcessor = KeGetCurrentProcessor()->Number,
        .ReadyProcessors = 0,
        .ReleaseProcessors = false,
        .Valid = false,
        .FailureCode = 0,
        .FailureValue1 = 0,
        .FailureValue2 = 0,
    };
    KeRequestIpiRoutine(VerifyPageIpi, &Context);
    MmFreePool(SeenPages, MM_POOL_TAG_PFN);
    if (!Context.Valid) {
        KdPrint(
            KD_TYPE_NONE,
            "M1TEST DIAGNOSTIC suite=memory-accounting verifier=%u value1=%llu value2=%llu\n",
            Context.FailureCode,
            Context.FailureValue1,
            Context.FailureValue2);
    }

    return Context.Valid;
}
#endif /* PALLADIUM_ENABLE_SELF_TESTS */

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
