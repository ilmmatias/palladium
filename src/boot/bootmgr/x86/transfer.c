/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <memory.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

[[noreturn]] void BiTransferExecution(uint64_t *Pml4, uint64_t EntryPoint);

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
    uint64_t EntryPoint) {
    /* TODO: Add support for PML5 (under compatible processors).
       We only care about the first 3 levels, as we're using large (2MiB pages). */

    do {
        uint64_t *Pml4 = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *EarlyIdentPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *LateIdentPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *KernelPdpt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *EarlyIdentPdt = BmAllocatePages(1, MEMORY_KERNEL);
        uint64_t *LateIdentPdt = BmAllocatePages(512, MEMORY_KERNEL);
        uint64_t *KernelPdt = BmAllocatePages(1, MEMORY_KERNEL);

        /* There is really no actual problem with big image sizes, we'd just need to allocate
           more PDTs if we wanted to, but just crash/assume invalid image for now.*/
        if (!Pml4 || !EarlyIdentPdpt || !LateIdentPdpt || !KernelPdpt || !EarlyIdentPdt ||
            !LateIdentPdt || !KernelPdt || ImageSize >= 0x40000000) {
            break;
        }

        /* First 2MiB are the early identity mapping (which contains ourselves; Required for no
           crash upon entering long mode).
           Right after the higher half barrier, we have the actual identity mapping (supporting up
           to 512 GiB, all mapped by default).
           The kernel is mapped at last, in whichever location the ASLR layer chose. */
        memset(Pml4, 0, PAGE_SIZE);
        Pml4[0] = (uint64_t)EarlyIdentPdpt | 0x03;
        Pml4[256] = (uint64_t)LateIdentPdpt | 0x03;
        Pml4[(VirtualAddress >> 39) & 0x1FF] = (uint64_t)KernelPdpt | 0x03;

        memset(EarlyIdentPdpt, 0, PAGE_SIZE);
        EarlyIdentPdpt[0] = (uint64_t)EarlyIdentPdt | 0x03;

        memset(LateIdentPdpt, 0, PAGE_SIZE);
        for (int i = 0; i < 512; i++) {
            LateIdentPdpt[i] = ((uint64_t)LateIdentPdt + (i << PAGE_SHIFT)) | 0x03;
        }

        memset(KernelPdpt, 0, PAGE_SIZE);
        KernelPdpt[(VirtualAddress >> 30) & 0x1FF] = (uint64_t)KernelPdt | 0x03;

        memset(EarlyIdentPdt, 0, PAGE_SIZE);
        EarlyIdentPdt[0] = 0x83;
        for (int i = 0; i < 262144; i++) {
            LateIdentPdt[i] = (i << LARGE_PAGE_SHIFT) | 0x83;
        }

        memset(KernelPdt, 0, PAGE_SIZE);
        for (uint64_t i = 0; i < ImageSize >> LARGE_PAGE_SHIFT; i++) {
            KernelPdt[((VirtualAddress >> 21) & 0x1FF) + i] =
                (PhysicalAddress + (i << LARGE_PAGE_SHIFT)) | 0x83;
        }

        BiTransferExecution(Pml4, EntryPoint);
    } while (0);

    while (1)
        ;
}
