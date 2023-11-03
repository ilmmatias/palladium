/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>
#include <ke.h>
#include <mi.h>
#include <rt.h>
#include <string.h>

extern uint64_t MiPoolStart;
extern RtBitmap MiPoolBitmap;

static const uint64_t PoolSize = 0x2000000000;
static const uint64_t PoolBitmapSize = PoolSize >> MM_PAGE_SHIFT;

static uint64_t *const Addresses[] = {
    (uint64_t *)0xFFFFFFFFFFFFF000,
    (uint64_t *)0xFFFFFFFFFFE00000,
    (uint64_t *)0xFFFFFFFFC0000000,
    (uint64_t *)0xFFFFFF8000000000,
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the architecture-dependent virtual memory (aka memory mapping) bits.
 *     After this, the pool allocator is ready to be initialized and used.
 *
 * PARAMETERS:
 *     LoaderData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializeVirtualMemory(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;

    MiPoolStart = 0xFFFF908000000000;

    RtInitializeBitmap(
        &MiPoolBitmap, (uint64_t *)BootData->MemoryManager.PoolBitmapBase, PoolBitmapSize);
    RtClearAllBits(&MiPoolBitmap);
}

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
uint64_t MiGetPhysicalAddress(void *VirtualAddress) {
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
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int MiMapPage(void *VirtualAddress, uint64_t PhysicalAddress, int Flags) {
    uint64_t Indexes[] = {
        ((uint64_t)VirtualAddress >> 39) & 0x1FF,
        ((uint64_t)VirtualAddress >> 30) & 0x3FFFF,
        ((uint64_t)VirtualAddress >> 21) & 0x7FFFFFF,
        ((uint64_t)VirtualAddress >> 12) & 0xFFFFFFFFF,
    };

    /* We need to move into the PTE (4KiB), allocating any pages along the way. */
    for (int i = 0; i < 3; i++) {
        if (!(Addresses[i][Indexes[i]] & 0x01)) {
            uint64_t Page = MmAllocatePages(1);
            if (!Page) {
                return 0;
            }

            memset(MI_PADDR_TO_VADDR(Page), 0, MM_PAGE_SIZE);
            Addresses[i][Indexes[i]] = Page | 0x03;

            void *NextLevel = (char *)Addresses[i + 1] + (Indexes[i] << MM_PAGE_SHIFT);
            __asm__ volatile("invlpg %0" :: "m"(NextLevel) : "memory");
        }
    }

    /* W^X is enforced higher up (random drivers shouldn't be acessing us!), we just
       convert the flags 1:1. */
    int PageFlags = 0x01;

    if (Flags & MI_MAP_WRITE) {
        PageFlags |= 0x02;
    }

    if (!(Flags & MI_MAP_EXEC)) {
        PageFlags |= 0x8000000000000000;
    }

    Addresses[3][Indexes[3]] = PhysicalAddress | PageFlags;
    __asm__ volatile("invlpg %0" :: "m"(VirtualAddress) : "memory");

    return 1;
}
