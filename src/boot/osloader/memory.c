/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <crt_impl/rand.h>
#include <efi/spec.h>
#include <efi/types.h>
#include <memory.h>
#include <rt/bitmap.h>
#include <stddef.h>
#include <stdint.h>

static void *BitmapBuffer = NULL;
static RtBitmap BitmapHeader = {0};
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
    EFI_STATUS Status =
        gBS->AllocatePool(EfiLoaderData, ((SpaceSize + 63) >> 6) << 3, &BitmapBuffer);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to allocate space for the virtual memory bitmap.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return Status;
    }

    RtInitializeBitmap(&BitmapHeader, BitmapBuffer, SpaceSize);
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
    if (!Pages || Pages > SpaceSize) {
        return NULL;
    }

    uint64_t StartingIndex =
        RtFindClearBitsAndSet(&BitmapHeader, __rand64() % (SpaceSize - Pages + 1), Pages);
    if (StartingIndex != (uint64_t)-1) {
        return (void *)(VIRTUAL_BASE + (StartingIndex << VIRTUAL_RANDOM_SHIFT));
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates memory using the firmware and stores the specified data for building
 *     the memory map descriptors later.
 *
 * PARAMETERS:
 *     Size - How many bytes to allocate.
 *     Alignment - Minimum required byte alignment for the address; should be a power of two, with
 *                 0 being accepted too (no alignment required).
 *
 * RETURN VALUE:
 *     Allocated address, or NULL if either request is invalid or the firmware fails to allocate
 *     the memory.
 *-----------------------------------------------------------------------------------------------*/
void *OslAllocatePages(size_t Size, uint64_t Alignment) {
    /* The firmware only lets us allocate page aligned addresses, so we gotta work around that by
     * overallocating then freeing the excess. EDK2 (MdePkg) actually uses the same method for its
     * AllocateAlignedPages function.*/
    if (!Size || (Alignment && (Alignment & (Alignment - 1)))) {
        return NULL;
    }

    UINTN Pages = Size >> EFI_PAGE_SHIFT;
    if (Size & EFI_PAGE_MASK) {
        Pages++;
    }

    if (Pages > (UINT64_MAX >> EFI_PAGE_SHIFT)) {
        return NULL;
    }

    EFI_PHYSICAL_ADDRESS AlignedAddress;
    EFI_STATUS Status;

    if (Alignment <= EFI_PAGE_SIZE) {
        Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, Pages, &AlignedAddress);
        if (Status != EFI_SUCCESS) {
            return NULL;
        }

        uint64_t AllocationSize = Pages << EFI_PAGE_SHIFT;
        if ((AlignedAddress & EFI_PAGE_MASK) || AlignedAddress > UINT64_MAX - AllocationSize) {
            if (!(AlignedAddress & EFI_PAGE_MASK)) {
                gBS->FreePages(AlignedAddress, Pages);
            }

            return NULL;
        }
    } else {
        EFI_PHYSICAL_ADDRESS UnalignedAddress;
        UINTN AlignmentPages = Alignment >> EFI_PAGE_SHIFT;
        if (Pages > UINT64_MAX - AlignmentPages) {
            return NULL;
        }

        UINTN TotalPages = Pages + AlignmentPages;
        if (TotalPages > (UINT64_MAX >> EFI_PAGE_SHIFT)) {
            return NULL;
        }

        Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, TotalPages, &UnalignedAddress);
        if (Status != EFI_SUCCESS) {
            return NULL;
        }

        uint64_t TotalSize = TotalPages << EFI_PAGE_SHIFT;
        if ((UnalignedAddress & EFI_PAGE_MASK) || UnalignedAddress > UINT64_MAX - (Alignment - 1) ||
            UnalignedAddress > UINT64_MAX - TotalSize) {
            if (!(UnalignedAddress & EFI_PAGE_MASK)) {
                gBS->FreePages(UnalignedAddress, TotalPages);
            }

            return NULL;
        }

        AlignedAddress = (UnalignedAddress + Alignment - 1) & ~(Alignment - 1);

        EFI_PHYSICAL_ADDRESS HeadStart = UnalignedAddress;
        EFI_PHYSICAL_ADDRESS HeadEnd = AlignedAddress;
        UINTN HeadPages = (HeadEnd - HeadStart) >> EFI_PAGE_SHIFT;
        if (HeadPages) {
            gBS->FreePages(HeadStart, HeadPages);
        }

        EFI_PHYSICAL_ADDRESS TailStart = AlignedAddress + (Pages << EFI_PAGE_SHIFT);
        EFI_PHYSICAL_ADDRESS TailEnd = UnalignedAddress + TotalSize;
        UINTN TailPages = (TailEnd - TailStart) >> EFI_PAGE_SHIFT;
        if (TailPages) {
            gBS->FreePages(TailStart, TailPages);
        }
    }

    return (void *)AlignedAddress;
}
