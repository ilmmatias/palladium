/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
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
    /* Align to the page boundary. */
    uint64_t Source = PhysicalAddress & ~(HALP_PT_SIZE - 1);
    uint64_t End = (PhysicalAddress + Size + HALP_PT_SIZE - 1) & ~(HALP_PT_SIZE - 1);

    /* Convert the type into proper flags. */
    int Flags = MI_MAP_WRITE | MI_MAP_UC;
    if (Type == MM_SPACE_IO) {
        Flags |= MI_MAP_UC;
    } else if (Type == MM_SPACE_GRAPHICS) {
        Flags |= MI_MAP_WC;
    }

    /* Grab some virtual memory.  */
    void *VirtualAddress = MiAllocatePoolSpace((End - Source) >> MM_PAGE_SHIFT);
    if (!VirtualAddress) {
        return NULL;
    }

    /* And now we just delegate to the HAL to do the actual mapping. */
    if (!HalpMapContiguousPages(VirtualAddress, Source, End - Source, Flags)) {
        /* TODO: If HalpMapContiguousPages returns false, we're left in an annoying situation where
         * we have no idea how much memory we need to unmap, so we probably should find a better way
         * of handling this than just leaking the virtual space... */
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
    uintptr_t Source = (uintptr_t)VirtualAddress & ~(HALP_PT_SIZE - 1);
    uintptr_t End = ((uintptr_t)VirtualAddress + Size + HALP_PT_SIZE - 1) & ~(HALP_PT_SIZE - 1);
    HalpUnmapPages((void *)Source, End - Source);
    MiFreePoolSpace((void *)Source, (End - Source) >> MM_PAGE_SHIFT);
}
