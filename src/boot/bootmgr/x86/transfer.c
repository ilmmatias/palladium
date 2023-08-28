/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86/bios.h>

#define LOADER_MAGIC "BMGR"
#define LOADER_CURRENT_VERSION 0x0000

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint16_t Version;
    struct {
        uint64_t MemorySize;
        uint64_t PageAllocatorBase;
    } MemoryManager;
    struct {
        uint64_t BaseAddress;
        uint32_t Count;
    } MemoryMap;
    struct {
        uint64_t BaseAddress;
        uint32_t Count;
    } Images;
} LoaderBootData;

extern BiosMemoryRegion *BiosMemoryMap;
extern uint32_t BiosMemoryMapEntries;
extern uint64_t BiosMemorySize;

[[noreturn]] void BiTransferExecution(uint64_t *Pml4, uint64_t BootData, uint64_t EntryPoint);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function maps up a kernel or kernel driver page, allocating and filling the page map
 *     along the way.
 *
 * PARAMETERS:
 *     Pml4 - Base physical address of the page map.
 *     VirtualAddress - Virtual address/destination for the page.
 *     PhysicalAddress - Physical page where the data is located.
 *     PageFlags - Page flags (from boot.h), will be converted into valid flags by us.
 *
 * RETURN VALUE:
 *     0 on failure, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int
MapPage(uint64_t *Pml4, uint64_t VirtualAddress, uint64_t PhysicalAddress, int PageFlags) {
    uint64_t Pml4Entry = (VirtualAddress >> 39) & 0x1FF;
    uint64_t PdptEntry = (VirtualAddress >> 30) & 0x1FF;
    uint64_t PdtEntry = (VirtualAddress >> 21) & 0x1FF;
    uint64_t PtEntry = (VirtualAddress >> 12) & 0x1FF;

    uint64_t *Pdpt = (uint64_t *)(Pml4[Pml4Entry] & ~0x7FF8000000000FFF);
    if (!(Pml4[Pml4Entry] & 0x01)) {
        Pdpt = (uint64_t *)BmAllocatePages(1, MEMORY_KERNEL);
        Pml4[Pml4Entry] = (uint64_t)Pdpt | 0x03;
        if (!Pdpt) {
            return 0;
        }
    }

    uint64_t *Pdt = (uint64_t *)(Pdpt[PdptEntry] & ~0x7FF8000000000FFF);
    if (!(Pdpt[PdptEntry] & 0x01)) {
        Pdt = (uint64_t *)BmAllocatePages(1, MEMORY_KERNEL);
        Pdpt[PdptEntry] = (uint64_t)Pdt | 0x03;
        if (!Pdt) {
            return 0;
        }
    }

    uint64_t *Pt = (uint64_t *)(Pdt[PdtEntry] & ~0x7FF8000000000FFF);
    if (!(Pdt[PdtEntry] & 0x01)) {
        Pt = (uint64_t *)BmAllocatePages(1, MEMORY_KERNEL);
        Pdt[PdtEntry] = (uint64_t)Pt | 0x03;
        if (!Pt) {
            return 0;
        }
    }

    Pt[PtEntry] = PhysicalAddress | (PageFlags & PAGE_WRITE     ? 0x8000000000000003
                                     : !(PageFlags & PAGE_EXEC) ? 0x8000000000000001
                                                                : 0x01);

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the paging structures, and prepares to put the processor in the
 *     correct state to jump into the kernel.
 *
 * PARAMETERS:
 *     Images - List containing all memory regions we need to map.
 *     ImageCount - How many entries the image list has.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmTransferExecution(LoadedImage *Images, size_t ImageCount) {
    /* TODO: Add support for PML5 (under compatible processors). */

    /* Pre-allocate the space required for the kernel's physical memory manager; This way
       the kernel doesn't need to worry about anything but filling the info.
       MmSize should always be `PagesOfMemory * sizeof(MmPageEntry)`, if MmPageEntry changes
       on the kernel, the size here needs to change too! */
    uint64_t PagesOfMemory = (BiosMemorySize + PAGE_SIZE - 1) >> PAGE_SHIFT;
    uint64_t MmSize = PagesOfMemory * 29;
    void *MmBase = BmAllocatePages((MmSize + PAGE_SIZE - 1) >> PAGE_SHIFT, MEMORY_KERNEL);
    if (!MmBase) {
        BmPanic("An error occoured while trying to load the selected operating system.\n"
                "There is not enough RAN for the memory manager.");
    }

    do {
        int Fail = 0;
        uint64_t *Pml4 = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *EarlyIdentPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *LateIdentPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *EarlyIdentPdt = BmAllocatePages(1, MEMORY_KERNEL);
        LoaderBootData *BootData = malloc(sizeof(LoaderBootData));

        if (!Pml4 || !EarlyIdentPdpt || !LateIdentPdpt || !EarlyIdentPdt || !BootData) {
            break;
        }

        /* First 2MiB are the early identity mapping (which contains ourselves; Required for no
           crash upon entering long mode).
           Right after the higher half barrier, we have the actual identity mapping (supporting up
           to 512 GiB, all mapped by default).
           After that, we have the recursive mapping of the paging structs (for fast access).
           The kernel and the kernel drivers are mapped at last, in whichever locations the ASLR
           layer chose. */
        memset(Pml4, 0, PAGE_SIZE);
        Pml4[0] = (uint64_t)EarlyIdentPdpt | 0x03;
        Pml4[256] = (uint64_t)LateIdentPdpt | 0x03;

        memset(EarlyIdentPdpt, 0, PAGE_SIZE);
        EarlyIdentPdpt[0] = (uint64_t)EarlyIdentPdt | 0x03;

        memset(LateIdentPdpt, 0, PAGE_SIZE);
        for (uint64_t i = 0; i < 512; i++) {
            LateIdentPdpt[i] = (i * 0x40000000) | 0x8000000000000083;
        }

        memset(EarlyIdentPdt, 0, PAGE_SIZE);
        EarlyIdentPdt[0] = 0x83;

        for (size_t i = 0; !Fail && i < ImageCount; i++) {
            for (size_t Page = 0; Page < Images[i].ImageSize >> PAGE_SHIFT; Page++) {
                if (!MapPage(
                        Pml4,
                        Images[i].VirtualAddress + (Page << PAGE_SHIFT),
                        Images[i].PhysicalAddress + (Page << PAGE_SHIFT),
                        Images[i].PageFlags[Page])) {
                    Fail = 1;
                    break;
                }
            }
        }

        if (Fail) {
            break;
        }

        /* Mount the OS-specific boot data, and enter assembly land to get into long mode.  */

        memcpy(BootData->Magic, LOADER_MAGIC, 4);
        BootData->Version = LOADER_CURRENT_VERSION;
        BootData->MemoryManager.MemorySize = BiosMemorySize;
        BootData->MemoryManager.PageAllocatorBase = (uint64_t)MmBase + 0xFFFF800000000000;
        BootData->MemoryMap.BaseAddress = (uint64_t)BiosMemoryMap + 0xFFFF800000000000;
        BootData->MemoryMap.Count = BiosMemoryMapEntries;
        BootData->Images.BaseAddress = (uint64_t)Images + 0xFFFF800000000000;
        BootData->Images.Count = ImageCount;

        BiTransferExecution(Pml4, (uint64_t)BootData + 0xFFFF800000000000, Images[0].EntryPoint);
    } while (0);

    BmPanic("An error occoured while trying to load the selected operating system.\n"
            "Please, reboot your device and try again.\n");
}
