/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mi.h>
#include <rt/bitmap.h>
#include <string.h>
#include <vid.h>

extern MiPageEntry *MiPageList;
extern MiPageEntry *MiFreePageListHead;

extern uint64_t MiPoolStart;
extern RtBitmap MiPoolBitmap;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a given amount of contiguous pages directly from the osloader
 *     memory map; This should only be used to initialize the pool and the PFN.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void *EarlyAllocatePages(KiLoaderBlock *LoaderBlock, uint32_t Pages) {
    RtDList *MemoryDescriptorListHead =
        MiEnsureEarlySpace((uint64_t)LoaderBlock->MemoryDescriptorListHead, sizeof(RtDList));

    for (RtDList *ListHeader = MiEnsureEarlySpace(
             (uint64_t)MemoryDescriptorListHead->Next, sizeof(MiMemoryDescriptor));
         ListHeader != MemoryDescriptorListHead;
         ListHeader = MiEnsureEarlySpace((uint64_t)ListHeader->Next, sizeof(MiMemoryDescriptor))) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

        if (Entry->PageCount < Pages ||
            (Entry->Type != MI_PAGE_FREE && Entry->Type != MI_PAGE_FIRMWARE_TEMPORARY)) {
            continue;
        }

        /* We need to make sure we don't add the low 64KiB to the free page; They are reserved
         * for if the kernel needs a low fixed memory address for something (temporary ofc). */
        if (Entry->BasePage < 0x10) {
            if (Entry->BasePage + Entry->PageCount < 0x10) {
                continue;
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

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the physical page allocator (and the page database).
 *     We mark all UEFI temporary and normal system memory regions as free; But we can't mark
 *     OSLOADER regions as free just yet (everything from LoaderBlock is inside them).
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePageAllocator(KiLoaderBlock *LoaderBlock) {
    /* The PFN database only tracks pages we might allocate, find the max addressable FREE
     * page. */
    RtDList *MemoryDescriptorListHead =
        MiEnsureEarlySpace((uint64_t)LoaderBlock->MemoryDescriptorListHead, sizeof(RtDList));
    uint32_t MaxAddressablePage = 0;

    for (RtDList *ListHeader = MiEnsureEarlySpace(
             (uint64_t)MemoryDescriptorListHead->Next, sizeof(MiMemoryDescriptor));
         ListHeader != MemoryDescriptorListHead;
         ListHeader = MiEnsureEarlySpace((uint64_t)ListHeader->Next, sizeof(MiMemoryDescriptor))) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);
        if (Entry->Type == MI_PAGE_FREE || Entry->Type == MI_PAGE_OSLOADER ||
            Entry->Type == MI_PAGE_FIRMWARE_TEMPORARY) {
            MaxAddressablePage = Entry->BasePage + Entry->PageCount;
        }
    }

    /* Find a memory map entry with enough space for the PFN database. We're assuming such entry
     * exists, we'll crash with a NULL dereference at some point if it doesn't. */
    uint32_t PfnDatabasePages =
        (MaxAddressablePage * sizeof(MiPageEntry) + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    MiPageList = EarlyAllocatePages(LoaderBlock, PfnDatabasePages);

    /* Setup the page allocator (marking the free pages as free). */
    for (RtDList *ListHeader = MiEnsureEarlySpace(
             (uint64_t)MemoryDescriptorListHead->Next, sizeof(MiMemoryDescriptor));
         ListHeader != MemoryDescriptorListHead;
         ListHeader = MiEnsureEarlySpace((uint64_t)ListHeader->Next, sizeof(MiMemoryDescriptor))) {
        MiMemoryDescriptor *Entry = CONTAINING_RECORD(ListHeader, MiMemoryDescriptor, ListHeader);

        if (Entry->Type != MI_PAGE_FREE && Entry->Type != MI_PAGE_FIRMWARE_TEMPORARY) {
            continue;
        }

        if (Entry->BasePage < 0x10) {
            if (Entry->BasePage + Entry->PageCount < 0x10) {
                continue;
            }

            Entry->PageCount -= 0x10 - Entry->BasePage;
            Entry->BasePage += 0x10 - Entry->BasePage;
        }

        MiPageEntry *Group = &MiPageList[Entry->BasePage];
        for (uint32_t i = 0; i < Entry->PageCount; i++) {
            MiPageEntry *Page = Group + i;
            Page->NextPage = MiFreePageListHead;
            Page->References = 0;
            MiFreePageListHead = Page;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the kernel pool allocator.
 *
 * PARAMETERS:
 *     BootData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePool(KiLoaderBlock *LoaderBlock) {
    uint64_t SizeInBits = (MI_POOL_SIZE + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    uint64_t SizeInPages = ((SizeInBits >> 3) + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;

    MiPoolStart = MI_POOL_START;

    void *PoolBitmapBase = EarlyAllocatePages(LoaderBlock, SizeInPages);
    RtInitializeBitmap(&MiPoolBitmap, PoolBitmapBase, SizeInBits);
    RtClearAllBits(&MiPoolBitmap);
}
