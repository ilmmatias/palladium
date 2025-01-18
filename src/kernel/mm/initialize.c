/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mi.h>
#include <rt/bitmap.h>
#include <string.h>
#include <vid.h>

extern MiPageEntry *MiPageList;
extern RtDList MiFreePageListHead;

extern uint64_t MiPoolStart;
extern RtBitmap MiPoolBitmap;

RtDList MiMemoryDescriptorListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries finding some space in the given memory descriptor.
 *
 * PARAMETERS:
 *     Entry - Which memory descriptor we're currently trying to use.
 *     Pages - How many contiguous pages we need.
 *
 * RETURN VALUE:
 *     Virtual address of the allocated area, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
static void *TryAllocatingPagesIn(MiMemoryDescriptor *Entry, uint64_t Pages) {
    if (Entry->PageCount < Pages ||
        (Entry->Type != MI_DESCR_FREE && Entry->Type != MI_DESCR_FIRMWARE_TEMPORARY)) {
        return NULL;
    }

    /* We need to make sure we won't use the low 64KiB; They are reserved if the kernel needs any
     * low memory (for initializing SMP or anything else like that). */
    if (Entry->BasePage < 0x10) {
        if (Entry->BasePage + Entry->PageCount < 0x10) {
            return NULL;
        }

        Entry->PageCount -= 0x10 - Entry->BasePage;
        Entry->BasePage += 0x10 - Entry->BasePage;
    }

    void *Result =
        MiEnsureEarlySpace((uint64_t)Entry->BasePage << MM_PAGE_SHIFT, Pages << MM_PAGE_SHIFT);
    Entry->BasePage += Pages;
    Entry->PageCount -= Pages;
    return Result;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a given amount of contiguous pages directly from the osloader
 *     memory map; This should only be used before the initialization of the pool and the PFN.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us. Set this to NULL if
 *                   MiSaveMemoryDescriptors has already been called.
 *     Pages - How many contiguous pages we need.
 *
 * RETURN VALUE:
 *     Virtual address of the allocated area, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *MiEarlyAllocatePages(KiLoaderBlock *LoaderBlock, uint64_t Pages) {
    if (LoaderBlock) {
        RtDList *MemoryDescriptorListHead =
            MiEnsureEarlySpace((uint64_t)LoaderBlock->MemoryDescriptorListHead, sizeof(RtDList));
        for (RtDList *ListHeader = MiEnsureEarlySpace(
                 (uint64_t)MemoryDescriptorListHead->Next, sizeof(MiMemoryDescriptor));
             ListHeader != MemoryDescriptorListHead;
             ListHeader =
                 MiEnsureEarlySpace((uint64_t)ListHeader->Next, sizeof(MiMemoryDescriptor))) {
            void *Result = TryAllocatingPagesIn(
                CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader), Pages);
            if (Result) {
                return Result;
            }
        }
    } else {
        for (RtDList *ListHeader = MiMemoryDescriptorListHead.Next;
             ListHeader != &MiMemoryDescriptorListHead;
             ListHeader = ListHeader->Next) {
            void *Result = TryAllocatingPagesIn(
                CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader), Pages);
            if (Result) {
                return Result;
            }
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates some early (non-osloader) space and copies over all the memory
 *     descriptors from loader block.
 *     This should be called before initializing the pool and page allocators.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiSaveMemoryDescriptors(KiLoaderBlock *LoaderBlock) {
    RtDList *MemoryDescriptorListHead =
        MiEnsureEarlySpace((uint64_t)LoaderBlock->MemoryDescriptorListHead, sizeof(RtDList));

    /* First, collect the amount of entries we have (and more specifically, how many bytes we need
     * to store them all). */
    uint64_t RequiredSpace = 0;
    for (RtDList *ListHeader = MiEnsureEarlySpace(
             (uint64_t)MemoryDescriptorListHead->Next, sizeof(MiMemoryDescriptor));
         ListHeader != MemoryDescriptorListHead;
         ListHeader = MiEnsureEarlySpace((uint64_t)ListHeader->Next, sizeof(MiMemoryDescriptor))) {
        RequiredSpace += sizeof(MiMemoryDescriptor);
    }

    /* Find a memory map entry with enough space for the descriptor list. We have no option but to
     * hang without any error messages if we fail here. */
    MiMemoryDescriptor *Descriptor =
        MiEarlyAllocatePages(LoaderBlock, (RequiredSpace + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT);
    if (!Descriptor) {
        while (1) {
            HalpStopProcessor();
        }
    }

    /* Now, copy over all the osloader data to kernel land. */
    RtInitializeDList(&MiMemoryDescriptorListHead);
    for (RtDList *ListHeader = MiEnsureEarlySpace(
             (uint64_t)MemoryDescriptorListHead->Next, sizeof(MiMemoryDescriptor));
         ListHeader != MemoryDescriptorListHead;
         ListHeader = MiEnsureEarlySpace((uint64_t)ListHeader->Next, sizeof(MiMemoryDescriptor))) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        memcpy(Descriptor, Entry, sizeof(MiMemoryDescriptor));
        RtAppendDList(&MiMemoryDescriptorListHead, &Descriptor->ListHeader);
        Descriptor++;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the physical page allocator (and the page database).
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
    for (RtDList *ListHeader = MiMemoryDescriptorListHead.Next;
         ListHeader != &MiMemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

        /* Unmapping firware temp regions should be already okay to do. */
        if (Entry->Type == MI_DESCR_FIRMWARE_TEMPORARY) {
            for (uint32_t i = 0; i < Entry->PageCount; i++) {
                HalpUnmapPage((void *)((uint64_t)(Entry->BasePage + i) << MM_PAGE_SHIFT));
            }
        }

        if (Entry->BasePage < 0x10) {
            if (Entry->BasePage + Entry->PageCount < 0x10) {
                continue;
            }

            Entry->PageCount -= 0x10 - Entry->BasePage;
            Entry->BasePage += 0x10 - Entry->BasePage;
        }

        if (Entry->Type <= MI_DESCR_FIRMWARE_TEMPORARY) {
            MaxAddressablePage = Entry->BasePage + Entry->PageCount;
        }
    }

    /* Find a memory map entry with enough space for the PFN database. We're assuming such entry
     * exists, we'll crash with a NULL dereference at some point if it doesn't. */
    MiPageList = MiEarlyAllocatePages(
        NULL, (MaxAddressablePage * sizeof(MiPageEntry) + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT);

    /* Setup the page allocator (marking the free pages as free). */
    RtInitializeDList(&MiFreePageListHead);
    for (RtDList *ListHeader = MiMemoryDescriptorListHead.Next;
         ListHeader != &MiMemoryDescriptorListHead;
         ListHeader = ListHeader->Next) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

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
    uint64_t SizeInBits = (MI_POOL_SIZE + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    uint64_t SizeInPages = ((SizeInBits >> 3) + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;

    MiPoolStart = MI_POOL_START;

    void *PoolBitmapBase = MiEarlyAllocatePages(NULL, SizeInPages);
    RtInitializeBitmap(&MiPoolBitmap, PoolBitmapBase, SizeInBits);
    RtClearAllBits(&MiPoolBitmap);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function wraps up the memory manager initialization by freeing and unmapping the
 *     OSLOADER regions. This should only be called after LoaderBlock (and anything else from
 *     OSLOADER) has already been used and saved somewhere else.
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
        if (Entry->Type != MI_DESCR_OSLOADER) {
            continue;
        }

        MiPageEntry *Group = &MiPageList[Entry->BasePage];
        for (uint64_t i = 0; i < (uint64_t)Entry->PageCount; i++) {
            Group[i].Flags = 0;
            RtPushDList(&MiFreePageListHead, &Group[i].ListHeader);
            HalpUnmapPage((void *)((uint64_t)(Entry->BasePage + i) << MM_PAGE_SHIFT));
        }
    }
}
