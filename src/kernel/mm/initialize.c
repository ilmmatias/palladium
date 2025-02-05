/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <rt/bitmap.h>
#include <string.h>

extern MiPageEntry *MiPageList;
extern RtDList MiFreePageListHead;

extern uint64_t MiPoolStart;
extern RtBitmap MiPoolBitmap;

static RtDList *LoaderDescriptors = NULL;

RtDList MiMemoryDescriptorListHead;

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
uint64_t MiAllocateEarlyPages(uint64_t Pages) {
    if (!LoaderDescriptors) {
        return 0;
    }

    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->PageCount < Pages || Entry->Type != MI_DESCR_FREE) {
            continue;
        }

        /* We need to make sure we won't use the low 64KiB; They are reserved if the kernel needs
         * any low memory (for initializing SMP or anything else like that). */
        if (Entry->BasePage < 0x10) {
            if (Entry->BasePage + Entry->PageCount < 0x10) {
                continue;
            }

            Entry->PageCount -= 0x10 - Entry->BasePage;
            Entry->BasePage += 0x10 - Entry->BasePage;
        }

        uint64_t Result = Entry->BasePage << MM_PAGE_SHIFT;
        Entry->BasePage += Pages;
        Entry->PageCount -= Pages;
        return Result;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function prepares for the memory manager initialization by setting up a really dummy
 *     page allocator that uses the osloader memory map directly. HalpMapPages should automatically
 *     use this during early boot (as needed).
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializeEarlyPageAllocator(KiLoaderBlock *LoaderBlock) {
    LoaderDescriptors = LoaderBlock->MemoryDescriptorListHead;
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

        /* Unmapping the 1:1 firware temp regions should be already okay to do. */
        if (Entry->Type == MI_DESCR_FIRMWARE_TEMPORARY ||
            Entry->Type == MI_DESCR_FIRMWARE_PERMANENT) {
            HalpUnmapPages(
                (void *)(Entry->BasePage << MM_PAGE_SHIFT), Entry->PageCount << MM_PAGE_SHIFT);
        }

        if (Entry->BasePage < 0x10) {
            if (Entry->BasePage + Entry->PageCount < 0x10) {
                continue;
            }

            Entry->PageCount -= 0x10 - Entry->BasePage;
            Entry->BasePage += 0x10 - Entry->BasePage;
        }

        if (Entry->Type <= MI_DESCR_FIRMWARE_PERMANENT) {
            MaxAddressablePage = Entry->BasePage + Entry->PageCount;
        }
    }

    /* Grab some physical memory and map it for the PFN database. This should be the last place we
     * need EarlyAllocatePages. */
    uint64_t Size = MaxAddressablePage * sizeof(MiPageEntry);
    uint64_t PhysicalAddress = MiAllocateEarlyPages((Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT);
    if (!PhysicalAddress) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    void *PageListBase = (void *)(MI_VIRTUAL_OFFSET + PhysicalAddress);
    if (!HalpMapPages(PageListBase, PhysicalAddress, Size, MI_MAP_WRITE)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_PFN_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    MiPageList = PageListBase;

    /* Setup the page allocator (marking the free pages as free). */
    RtInitializeDList(&MiFreePageListHead);
    for (RtDList *ListHeader = LoaderDescriptors->Next; ListHeader != LoaderDescriptors;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->Type > MI_DESCR_FIRMWARE_PERMANENT) {
            continue;
        }

        MiPageEntry *Group = &MiPageList[Entry->BasePage];

        if (Entry->Type != MI_DESCR_FREE && Entry->Type != MI_DESCR_FIRMWARE_TEMPORARY) {
            for (uint32_t i = 0; i < Entry->PageCount; i++) {
                Group[i].Flags = MI_PAGE_FLAGS_USED;
            }
        } else {
            for (uint32_t i = 0; i < Entry->PageCount; i++) {
                Group[i].Flags = 0;
                RtPushDList(&MiFreePageListHead, &Group[i].ListHeader);
            }
        }
    }

    /* Now we should be free to allocate some pool memory and copy the memory descriptor list in its
     * current state. */
    MiMemoryDescriptor *Descriptor = MmAllocatePool(MemoryDescriptorListSize, "KeMm");
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
 *     This function sets up the kernel pool allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePool(void) {
    MiPoolStart = MI_POOL_START;

    uint64_t SizeInBits = (MI_POOL_SIZE + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    uint64_t SizeInPages = ((SizeInBits >> 3) + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    uint64_t PhysicalAddress = MiAllocateEarlyPages(SizeInPages);
    if (!PhysicalAddress) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_POOL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    void *PoolBitmapBase = (void *)(MI_VIRTUAL_OFFSET + PhysicalAddress);
    if (!HalpMapPages(PoolBitmapBase, PhysicalAddress, SizeInBits >> 3, MI_MAP_WRITE)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_POOL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    RtInitializeBitmap(&MiPoolBitmap, PoolBitmapBase, SizeInBits);
    RtClearAllBits(&MiPoolBitmap);
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
            Group[i].Flags = 0;
            RtPushDList(&MiFreePageListHead, &Group[i].ListHeader);
        }

        HalpUnmapPages(
            (void *)(Entry->BasePage << MM_PAGE_SHIFT), Entry->PageCount << MM_PAGE_SHIFT);
    }
}
