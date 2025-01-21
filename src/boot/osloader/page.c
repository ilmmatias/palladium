/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <memory.h>

RtSList OslpAllocationListHead = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates memory using the firmware and stores the specified data for building
 *     the memory map descriptors later.
 *
 * PARAMETERS:
 *     Size - How many bytes to allocate.
 *     Alignment - Alignment mask for the address.
 *     Type - Which type to store into the allocation header.
 *
 * RETURN VALUE:
 *     Does not return on success.
 *-----------------------------------------------------------------------------------------------*/
void *OslAllocatePages(size_t Size, uint64_t Alignment, uint8_t Type) {
    /* The firmware only lets us allocate page aligned addresses, so we gotta work around that by
     * overallocating then freeing the excess. EDK2 (MdePkg) actually uses the same method for its
     * AllocateAlignedPages function.*/
    UINTN Pages = (Size + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT;
    EFI_PHYSICAL_ADDRESS AlignedAddress;
    EFI_STATUS Status;

    if (Alignment <= EFI_PAGE_SIZE) {
        Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, Pages, &AlignedAddress);
        if (Status != EFI_SUCCESS) {
            return NULL;
        }
    } else {
        EFI_PHYSICAL_ADDRESS UnalignedAddress;
        UINTN TotalPages = Pages + ((Alignment + EFI_PAGE_SIZE - 1) >> EFI_PAGE_SHIFT);
        Status = gBS->AllocatePages(AllocateAnyPages, EfiLoaderData, TotalPages, &UnalignedAddress);
        if (Status != EFI_SUCCESS) {
            return NULL;
        }

        AlignedAddress = (UnalignedAddress + Alignment - 1) & ~(Alignment - 1);

        UINTN HeadStart = UnalignedAddress;
        UINTN HeadEnd = AlignedAddress;
        UINTN HeadPages = (HeadEnd - HeadStart) >> EFI_PAGE_SHIFT;
        gBS->FreePages(HeadStart, HeadPages);

        UINTN TailStart = AlignedAddress + (Pages << EFI_PAGE_SHIFT);
        UINTN TailEnd = UnalignedAddress + (TotalPages << EFI_PAGE_SHIFT);
        UINTN TailPages = (TailEnd - TailStart) >> EFI_PAGE_SHIFT;
        gBS->FreePages(TailStart, TailPages);
    }

    OslpAllocation *Allocation = NULL;
    Status = gBS->AllocatePool(EfiLoaderData, sizeof(OslpAllocation), (VOID **)&Allocation);
    if (Status != EFI_SUCCESS) {
        gBS->FreePages(AlignedAddress, Pages);
        return NULL;
    }

    Allocation->Buffer = (void *)AlignedAddress;
    Allocation->Size = Size;
    Allocation->Type = Type;
    RtPushSList(&OslpAllocationListHead, &Allocation->ListHeader);

    return Allocation->Buffer;
}
