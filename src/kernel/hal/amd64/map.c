/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <mi.h>
#include <string.h>

extern MiPageEntry *MiPageList;

static uint64_t *const Addresses[] = {
    (uint64_t *)0xFFFFFFFFFFFFF000,
    (uint64_t *)0xFFFFFFFFFFE00000,
    (uint64_t *)0xFFFFFFFFC0000000,
    (uint64_t *)0xFFFFFF8000000000,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function grabs the physical address of the specified virtual address.
 *
 * PARAMETERS:
 *     VirtualAddress - Address we need the physical page of.
 *
 * RETURN VALUE:
 *     Physical address, or 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalpGetPhysicalAddress(void *VirtualAddress) {
    uint64_t Indexes[] = {
        ((uint64_t)VirtualAddress >> 39) & 0x1FF,
        ((uint64_t)VirtualAddress >> 30) & 0x3FFFF,
        ((uint64_t)VirtualAddress >> 21) & 0x7FFFFFF,
        ((uint64_t)VirtualAddress >> 12) & 0xFFFFFFFFF,
    };

    /* Levels of the table not existing is a fail for us. */
    for (int i = 0; i < 4; i++) {
        if (!(Addresses[i][Indexes[i]] & 0x01)) {
            return 0;
        }
    }

    return Addresses[3][Indexes[3]] & 0x000FFFFFFFFFF000;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps a physical addresses into virtual memory.
 *
 * PARAMETERS:
 *     VirtualAddress - Destination address.
 *     PhysicalAddress - Source address.
 *     Flags - How we want to map the page.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalpMapPage(void *VirtualAddress, uint64_t PhysicalAddress, int Flags) {
    uint64_t Indexes[] = {
        ((uint64_t)VirtualAddress >> 39) & 0x1FF,
        ((uint64_t)VirtualAddress >> 30) & 0x3FFFF,
        ((uint64_t)VirtualAddress >> 21) & 0x7FFFFFF,
        ((uint64_t)VirtualAddress >> 12) & 0xFFFFFFFFF,
    };

    /* We need to move into the PTE (4KiB), allocating any pages along the way. */
    for (int i = 0; i < 3; i++) {
        if (Addresses[i][Indexes[i]] & 0x80) {
            return true;
        }

        if (!(Addresses[i][Indexes[i]] & 0x01)) {
            uint64_t Page = MmAllocateSinglePage();
            if (!Page) {
                return false;
            }

            memset((void *)(Page + 0xFFFF800000000000), 0, MM_PAGE_SIZE);
            Addresses[i][Indexes[i]] = Page | 0x03;
        }
    }

    if (!(Addresses[3][Indexes[3]] & 0x01)) {
        /* W^X is enforced higher up (random drivers shouldn't be acessing us!), we just
        convert the flags 1:1. */
        int PageFlags = 0x01;

        if (Flags & MI_MAP_WRITE) {
            PageFlags |= 0x02;
        }

        if (Flags & MI_MAP_DEVICE) {
            PageFlags |= 0x80;
        }

        if (!(Flags & MI_MAP_EXEC)) {
            PageFlags |= 0x8000000000000000;
        }

        Addresses[3][Indexes[3]] = PhysicalAddress | PageFlags;
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unmaps a physical addresses from virtual memory.
 *
 * PARAMETERS:
 *     VirtualAddress - Target address.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpUnmapPage(void *VirtualAddress) {
    uint64_t Indexes[] = {
        ((uint64_t)VirtualAddress >> 39) & 0x1FF,
        ((uint64_t)VirtualAddress >> 30) & 0x3FFFF,
        ((uint64_t)VirtualAddress >> 21) & 0x7FFFFFF,
        ((uint64_t)VirtualAddress >> 12) & 0xFFFFFFFFF,
    };

    /* We just want to move along into the PTE this time around. */
    for (int i = 0; i < 4; i++) {
        if (!(Addresses[i][Indexes[i]] & 0x01)) {
            return;
        } else if (!(Addresses[i][Indexes[i]] & 0x80) && i != 3) {
            continue;
        }

        /* We're assuming that for large/huge pages, if the given VirtualAddress is properly aligned
         * we want to unmap the whole region. */
        if ((i == 1 && (Addresses[i][Indexes[i]] & 0x80) &&
             ((uint64_t)VirtualAddress & 0x3FFFFFFF)) ||
            (i == 2 && (Addresses[i][Indexes[i]] & 0x80) &&
             ((uint64_t)VirtualAddress & 0x1FFFFF))) {
            return;
        }

        if (Addresses[i][Indexes[i]] & 0x01) {
            Addresses[i][Indexes[i]] = 0;
        }

        break;
    }

    /* TODO: We also want to notify other processors. */
    __asm__ volatile("invlpg (%0)" : : "b"(VirtualAddress) : "memory");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps a range of physical addresses into contiguous virtual memory.
 *
 * PARAMETERS:
 *     PhysicalAddress - Source address.
 *     Size - Size in bytes of the region.
 *
 * RETURN VALUE:
 *     Pointer to the start of the virtual memory range on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *MmMapSpace(uint64_t PhysicalAddress, size_t Size) {
    void *VirtualAddress = (void *)(PhysicalAddress + 0xFFFF800000000000);

    /* Map any unmapped pages as read/write + writethrough (device); We don't need to allocate
     * any pool space, as we already have a 1-to-1 space in virtual memory for the mappings. */
    for (size_t i = 0; i < Size; i += MM_PAGE_SIZE) {
        uint64_t Source = (PhysicalAddress & ~(MM_PAGE_SIZE - 1)) + i;
        void *Target = (void *)(Source + 0xFFFF800000000000);
        if (HalpGetPhysicalAddress(Target)) {
            continue;
        } else if (!HalpMapPage(Target, Source, MI_MAP_WRITE | MI_MAP_DEVICE)) {
            return NULL;
        }
    }

    return VirtualAddress;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unmaps physical address previously mapped by MmMapSpace, do not use this on
 *     normal heap pages!
 *
 * PARAMETERS:
 *     VirtualAddress - Value previously returned by MmMapSpace.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MmUnmapSpace(void *VirtualAddress) {
    (void)VirtualAddress;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the equivalent of MmMapSpace for early boot.
 *     No memory allocation functions (such as MmAllocateSinglePage) are called.
 *     Addresses are assumed to be mapped to one address only (repeated calls with the same
 *     physical base will give the same virtual address), and failure to map will result in a
 *     panic.
 *
 * PARAMETERS:
 *     PhysicalAddress - Source address.
 *     Size - Size in bytes of the region.
 *
 * RETURN VALUE:
 *     Pointer to the start of the virtual memory range.
 *-----------------------------------------------------------------------------------------------*/
void *MiEnsureEarlySpace(uint64_t PhysicalAddress, size_t Size) {
    (void)Size;
    return (void *)(PhysicalAddress + 0xFFFF800000000000);
}
