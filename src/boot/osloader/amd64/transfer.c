/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <loader.h>
#include <platform.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function exits the EFI environment, and transfers execution into the kernel.
 *
 * PARAMETERS:
 *     BootBlock - Structure containing the information the kernel needs to initialize.
 *     Stack - Stack pointer for uses during the early kernel initialization.
 *     PageMap - Page map previously allocated by OslpCreatePageMap.
 *     MemoryMapSize - Size of the runtime services memory descriptor list.
 *     MemoryMap - Pointer to the memory descriptors.
 *     DescriptorSize - Size of each entry in the memory map,
 *     DescriptorVersion - Version of the memory map entry.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void OslpTransferExecution(
    OslpBootBlock *BootBlock,
    void *BootStack,
    void *PageMap,
    UINTN MemoryMapSize,
    EFI_MEMORY_DESCRIPTOR *MemoryMap,
    UINTN DescriptorSize,
    UINT32 DescriptorVersion) {
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
    __asm__ volatile("mov %0, %%cr3" : : "r"(PageMap));

    /* SetVirtualAddressMap expects us to fill up the VirtualStart in the EFI_MEMORY_DESCRIPTORs,
     * and then call it after ExitBootServices+loading up our page table, so let's do that. */
    for (UINTN Offset = 0; Offset < MemoryMapSize; Offset += DescriptorSize) {
        EFI_MEMORY_DESCRIPTOR *Descriptor = (EFI_MEMORY_DESCRIPTOR *)((uint64_t)MemoryMap + Offset);
        if (Descriptor->Attribute & EFI_MEMORY_RUNTIME) {
            Descriptor->VirtualStart = 0xFFFF800000000000 + Descriptor->PhysicalStart;
        }
    }

    /* Maybe we should do this in the kernel (to have access to the KeFatalError function)? */
    Status = gRT->SetVirtualAddressMap(MemoryMapSize, DescriptorSize, DescriptorVersion, MemoryMap);
    if (Status != EFI_SUCCESS) {
        while (1) {
            __asm__ volatile("hlt");
        }
    }

    /* At last, call/jump into to the kernel entry; We'll be using the temporary stack, and we
     * expect the kernel to create the BSP structure and start using its stack before freeing
     * the loader temporary entries. */
    void *EntryPoint =
        CONTAINING_RECORD(BootBlock->BootDriverListHead->Next, OslpModuleEntry, ListHeader)
            ->EntryPoint;

    __asm__ volatile("mov %0, %%rax\n"
                     "mov %1, %%rcx\n"
                     "mov %2, %%rsp\n"
                     "jmp *%%rax\n"
                     :
                     : "r"(EntryPoint), "r"(BootBlock), "r"(BootStack)
                     : "%rax", "%rcx");

    /* It should be impossible to get here (because we jmp'ed instead of call'ing). */
    while (1) {
        __asm__ volatile("hlt");
    }
}
