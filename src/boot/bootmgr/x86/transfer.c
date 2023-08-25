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
#define LOADER_CURRENT_VERSION 0x00

typedef struct __attribute__((packed)) {
    char Magic[4];
    uint8_t Version;
    struct {
        uint64_t BaseAddress;
        uint32_t Count;
    } MemoryMap;
} LoaderBootData;

extern BiosMemoryRegion *BiosMemoryMap;
extern uint32_t BiosMemoryMapEntries;

[[noreturn]] void BiTransferExecution(uint64_t *Pml4, uint64_t EntryPoint, uint64_t BootData);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the paging structures, and prepares to put the processor in the
 *     correct state to jump into the kernel.
 *
 * PARAMETERS:
 *     VirtualAddress - Higher half virtual address where the kernel should be located.
 *     PhysicalAddress - Physical location of the kernel.
 *     ImageSize - Rounded up size of all sections of the kernel + headers.
 *     EntryPoint - Virtual address where the kernel entry point should be located.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmTransferExecution(
    uint64_t VirtualAddress,
    uint64_t PhysicalAddress,
    uint64_t ImageSize,
    uint64_t EntryPoint,
    int *PageFlags) {
    /* TODO: Add support for PML5 (under compatible processors). */

    do {
        uint64_t *Pml4 = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *EarlyIdentPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *LateIdentPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *KernelPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *EarlyIdentPdt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *KernelPdt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *KernelPt = BmAllocatePages((ImageSize + 0x1FFFFF) >> 21, MEMORY_KERNEL);
        LoaderBootData *BootData = malloc(sizeof(LoaderBootData));

        if (!Pml4 || !EarlyIdentPdpt || !LateIdentPdpt || !KernelPdpt || !EarlyIdentPdt ||
            !KernelPdt || !BootData || ImageSize >= 0x40000000) {
            break;
        }

        /* First 2MiB are the early identity mapping (which contains ourselves; Required for no
           crash upon entering long mode).
           Right after the higher half barrier, we have the actual identity mapping (supporting up
           to 512 GiB, all mapped by default).
           After that, we have the recursive mapping of the paging structs (for fast access).
           The kernel is mapped at last, in whichever location the ASLR layer chose. */
        memset(Pml4, 0, PAGE_SIZE);
        Pml4[0] = (uint64_t)EarlyIdentPdpt | 0x03;
        Pml4[256] = (uint64_t)LateIdentPdpt | 0x03;
        Pml4[(VirtualAddress >> 39) & 0x1FF] = (uint64_t)KernelPdpt | 0x03;

        memset(EarlyIdentPdpt, 0, PAGE_SIZE);
        EarlyIdentPdpt[0] = (uint64_t)EarlyIdentPdt | 0x03;

        memset(LateIdentPdpt, 0, PAGE_SIZE);
        for (uint64_t i = 0; i < 512; i++) {
            LateIdentPdpt[i] = (i * 0x40000000) | 0x8000000000000083;
        }

        memset(KernelPdpt, 0, PAGE_SIZE);
        KernelPdpt[(VirtualAddress >> 30) & 0x1FF] = (uint64_t)KernelPdt | 0x03;

        memset(EarlyIdentPdt, 0, PAGE_SIZE);
        EarlyIdentPdt[0] = 0x83;

        memset(KernelPdt, 0, PAGE_SIZE);
        for (uint64_t i = 0; i < (ImageSize + 0x1FFFFF) >> 21; i++) {
            KernelPdt[((VirtualAddress >> 21) & 0x1FF) + i] = (uint64_t)(KernelPt + i * 512) | 0x03;
        }

        memset(KernelPt, 0, PAGE_SIZE);
        for (uint64_t i = 0; i < ImageSize >> PAGE_SHIFT; i++) {
            KernelPt[((VirtualAddress >> 12) & 0x1FF) + i] =
                (PhysicalAddress + (i << PAGE_SHIFT)) |
                (PageFlags[i] & PAGE_WRITE     ? 0x8000000000000003
                 : !(PageFlags[i] & PAGE_EXEC) ? 0x8000000000000001
                                               : 0x01);
        }

        /* Mount the OS-specific boot data, and enter assembly land to get into long mode.  */

        memcpy(BootData->Magic, LOADER_MAGIC, 4);
        BootData->Version = LOADER_CURRENT_VERSION;
        BootData->MemoryMap.BaseAddress = (uint64_t)BiosMemoryMap + 0xFFFF800000000000;
        BootData->MemoryMap.Count = BiosMemoryMapEntries;

        BiTransferExecution(Pml4, EntryPoint, (uint64_t)BootData + 0xFFFF800000000000);
    } while (0);

    BmPanic("An error occoured while trying to load the selected operating system.\n"
            "Please, reboot your device and try again.\n");
}
