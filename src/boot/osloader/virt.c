/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <crt_impl.h>
#include <memory.h>
#include <rt/bitmap.h>

static void *BitmapBuffer = NULL;
static RtBitmap BitmapHeader = {};
static uint64_t SpaceSize = 1 << VIRTUAL_RANDOM_BITS;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the virtual address space allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
EFI_STATUS OslpInitializeVirtualAllocator(void) {
    EFI_STATUS Status = gBS->AllocatePool(EfiLoaderData, SpaceSize >> 3, &BitmapBuffer);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to allocate space for the virtual memory bitmap.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return Status;
    }

    RtInitializeBitmap(&BitmapHeader, (uint64_t *)BitmapBuffer, SpaceSize);
    RtClearAllBits(&BitmapHeader);
    return Status;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a range of virtual addresses, randomizing the high bits if possible.
 *
 * PARAMETERS:
 *     Pages - Amount of pages (EFI_PAGE_SIZE); This will be aligned (up) based on the
 *             VIRTUAL_RANDOM_PAGES value.
 *
 * RETURN VALUE:
 *     Allocated address, or NULL if no address was found.
 *-----------------------------------------------------------------------------------------------*/
void *OslAllocateVirtualAddress(uint64_t Pages) {
    Pages = (Pages + VIRTUAL_RANDOM_PAGES - 1) / VIRTUAL_RANDOM_PAGES;
    uint64_t StartingIndex =
        RtFindClearBitsAndSet(&BitmapHeader, __rand64() % (SpaceSize - Pages), Pages);
    if (StartingIndex != (uint64_t)-1) {
        return (void *)(VIRTUAL_BASE + (StartingIndex << VIRTUAL_RANDOM_SHIFT));
    } else {
        return NULL;
    }
}
