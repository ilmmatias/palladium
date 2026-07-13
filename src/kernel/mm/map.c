/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/mi.h>
#include <kernel/mm.h>
#include <stddef.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps a range of physical addresses into contiguous virtual memory.
 *
 * PARAMETERS:
 *     Type - Memory type for the mapping.
 *     PhysicalAddress - Source address, does not need to be aligned to MM_PAGE_SIZE.
 *     Size - Size in bytes of the region (also doesn't need to be aligned/a multiple of
 *            MM_PAGE_SIZE).
 *
 * RETURN VALUE:
 *     Pointer to the start of the virtual memory range on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *MmMapSpace(int Type, uint64_t PhysicalAddress, size_t Size) {
    if (!Size || PhysicalAddress > UINT64_MAX - Size) {
        return NULL;
    }

    /* Align to the page boundary. */
    uint64_t Source = PhysicalAddress & ~(HALP_PT_SIZE - 1);
    uint64_t UnalignedEnd = PhysicalAddress + Size;
    if (UnalignedEnd > UINT64_MAX - (HALP_PT_SIZE - 1)) {
        return NULL;
    }

    uint64_t End = (UnalignedEnd + HALP_PT_SIZE - 1) & ~(HALP_PT_SIZE - 1);
    uint64_t Pages = (End - Source) >> MM_PAGE_SHIFT;
    if (Pages > UINT32_MAX) {
        return NULL;
    }

    /* Convert the type into proper flags. */
    int Flags = MI_MAP_WRITE | MI_MAP_UC;
    if (Type == MM_SPACE_IO) {
        Flags |= MI_MAP_UC;
    } else if (Type == MM_SPACE_GRAPHICS) {
        Flags |= MI_MAP_WC;
    }

    /* Grab some virtual memory.  */
    void *VirtualAddress = MiAllocatePoolSpace(Pages);
    if (!VirtualAddress) {
        return NULL;
    }

    /* And now we just delegate to the HAL to do the actual mapping. */
    if (!HalpMapContiguousPages(VirtualAddress, Source, End - Source, Flags)) {
        /* Pool virtual space is freshly allocated, so it cannot contain a preexisting mapping.
         * Unmapping the complete requested range rolls back any prefix the HAL installed before
         * reporting failure, including now-empty page-table levels. */
        HalpUnmapPages(VirtualAddress, End - Source);
        MiFreePoolSpace(VirtualAddress, Pages);
        return NULL;
    }

    return (char *)VirtualAddress + PhysicalAddress - Source;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unmaps physical address previously mapped by MmMapSpace, do not use this on
 *     normal heap pages!
 *
 * PARAMETERS:
 *     VirtualAddress - Value previously returned by MmMapSpace.
 *     Size - Size in bytes of the region (same input as passed to MmMapSpace).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MmUnmapSpace(void *VirtualAddress, size_t Size) {
    if (!Size || (uintptr_t)VirtualAddress > UINTPTR_MAX - Size) {
        return;
    }

    uintptr_t Source = (uintptr_t)VirtualAddress & ~(HALP_PT_SIZE - 1);
    uintptr_t UnalignedEnd = (uintptr_t)VirtualAddress + Size;
    if (UnalignedEnd > UINTPTR_MAX - (HALP_PT_SIZE - 1)) {
        return;
    }

    uintptr_t End = (UnalignedEnd + HALP_PT_SIZE - 1) & ~(HALP_PT_SIZE - 1);
    uint64_t Pages = (End - Source) >> MM_PAGE_SHIFT;
    if (Pages > UINT32_MAX) {
        return;
    }

    HalpUnmapPages((void *)Source, End - Source);
    MiFreePoolSpace((void *)Source, Pages);
}
