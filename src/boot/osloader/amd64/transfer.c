/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <efi/spec.h>
#include <loader.h>
#include <platform.h>

extern uint64_t *OslpPageMap;
extern void *OslpGetBootStack(void *BspPtr);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function exits the EFI environment, and transfers execution into the kernel.
 *
 * PARAMETERS:
 *     BootBlock - Structure containing the information the kernel needs to initialize.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void OslpTransferExecution(OslpBootBlock *BootBlock) {
    /* At some point or another, this should return EFI_SUCCESS, or so we hope. */

    EFI_STATUS Status;
    do {
        UINTN MapKey = 0;
        UINTN MemoryMapSize = 0;
        UINTN DescriptorSize = 0;
        UINT32 DescriptorVersion = 0;
        gBS->GetMemoryMap(&MemoryMapSize, NULL, &MapKey, &DescriptorSize, &DescriptorVersion);
        Status = gBS->ExitBootServices(gIH, MapKey);
    } while (Status != EFI_SUCCESS);

    /* GDT and IDT address won't be sane after we load the new page table; We need to disable
     * interrupts. */
    __asm__ volatile("cli");

    /* We almost certainly want PSE (Page Size Extension) enabled. */
    __asm__ volatile("mov %%cr4, %%rax\n"
                     "or $0x08, %%rax\n"
                     "mov %%rax, %%cr4\n"
                     :
                     :
                     : "%rax");

    /* NX (No Execute) too; QEMU/KVM at least will crash if we try execution something with the NX
     * bit set if we don't enable it. */
    __asm__ volatile("mov $0xC0000080, %%rcx\n"
                     "rdmsr\n"
                     "or $0x800, %%rax\n"
                     "wrmsr\n"
                     :
                     :
                     : "%rax", "%rcx");

    /* Set the 4th PAT entry to WC (we use that for devices+physical memory at
     * 0xFFFF800000000000). */
    __asm__ volatile("mov $0x277, %%rcx\n"
                     "rdmsr\n"
                     "and $0xFFFFFF00, %%edx\n"
                     "or $0x01, %%edx\n"
                     "wrmsr\n"
                     :
                     :
                     : "%rax", "%rcx");

    /* We can load up the new page table now. */
    __asm__ volatile("mov %0, %%cr3" : : "r"(OslpPageMap));

    /* Load up the kernel stack, and call/jump to the kernel entry;
     * The kernel stack and the kernel entry will already be virtual addresses, but we expect
     * the kernel to convert everything else to virtual addresses (and probably relocate it). */
    uint64_t BootStack = (uint64_t)OslpGetBootStack(BootBlock) + 0xFFFF800000000000;
    void *EntryPoint =
        CONTAINING_RECORD(BootBlock->BootDriverListHead->Next, OslpModuleEntry, ListHeader)
            ->EntryPoint;
    __asm__ volatile(
        "mov %0, %%rax\n"
        "mov %1, %%rcx\n"
        "mov %2, %%rdx\n"
        "mov %3, %%rsp\n"
        "jmp *%%rax\n"
        :
        : "r"(EntryPoint), "r"(BootBlock), "r"(BootBlock->BootProcessor), "r"(BootStack)
        : "%rax", "%rcx", "%rdx");

    while (1)
        ;
}
