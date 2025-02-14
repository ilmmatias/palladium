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
    /* Before doing anything, let's fix all pointers inside the boot block (they are all physical,
     * but the kernel expects valid virtual addresses). */
    RtDList *ListHead = BootBlock->MemoryDescriptorListHead;
    RtDList *ListHeader = ListHead->Next;
    BootBlock->MemoryDescriptorListHead = (RtDList *)((uint64_t)ListHead + 0xFFFF800000000000);
    ListHead->Prev = (RtDList *)((uint64_t)ListHead->Prev + 0xFFFF800000000000);
    ListHead->Next = (RtDList *)((uint64_t)ListHead->Next + 0xFFFF800000000000);
    while (ListHeader != ListHead) {
        RtDList *Next = ListHeader->Next;
        ListHeader->Prev = (RtDList *)((uint64_t)ListHeader->Prev + 0xFFFF800000000000);
        ListHeader->Next = (RtDList *)((uint64_t)ListHeader->Next + 0xFFFF800000000000);
        ListHeader = Next;
    }

    /* We need to save the kernel module entry for ourselves as well (because we're about to lose
     * access to it). */
    ListHead = BootBlock->BootDriverListHead;
    ListHeader = ListHead->Next;
    void *EntryPoint = CONTAINING_RECORD(ListHeader, OslpModuleEntry, ListHeader)->EntryPoint;
    BootBlock->BootDriverListHead = (RtDList *)((uint64_t)ListHead + 0xFFFF800000000000);
    ListHead->Prev = (RtDList *)((uint64_t)ListHead->Prev + 0xFFFF800000000000);
    ListHead->Next = (RtDList *)((uint64_t)ListHead->Next + 0xFFFF800000000000);
    while (ListHeader != ListHead) {
        RtDList *Next = ListHeader->Next;
        ListHeader->Prev = (RtDList *)((uint64_t)ListHeader->Prev + 0xFFFF800000000000);
        ListHeader->Next = (RtDList *)((uint64_t)ListHeader->Next + 0xFFFF800000000000);
        OslpModuleEntry *Module = CONTAINING_RECORD(ListHeader, OslpModuleEntry, ListHeader);
        Module->ImageName = (char *)((uint64_t)Module->ImageName + 0xFFFF800000000000);
        ListHeader = Next;
    }

    BootBlock->BackBuffer = (void *)((uint64_t)BootBlock->BackBuffer + 0xFFFF800000000000);
    BootBlock->FrontBuffer = (void *)((uint64_t)BootBlock->FrontBuffer + 0xFFFF800000000000);
    BootBlock = (OslpBootBlock *)((uint64_t)BootBlock + 0xFFFF800000000000);

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

    /* Enable SSE; We'll loadup some sane defaults after we switch our stack. */
    __asm__ volatile("fninit\n"
                     "mov %%cr0, %%rax\n"
                     "and $0xFFFB, %%ax\n"
                     "or $0x02, %%rax\n"
                     "mov %%rax, %%cr0\n"
                     "mov %%cr4, %%rax\n"
                     "or $0x600, %%rax\n"
                     "mov %%rax, %%cr4\n"
                     :
                     :
                     : "%rax");

    /* We almost certainly want PSE (Page Size Extension) enabled. */
    __asm__ volatile("mov %%cr4, %%rax\n"
                     "or $0x10, %%rax\n"
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

    /* Set the 4th PAT entry to WC (for the graphics buffer). */
    __asm__ volatile("wbinvd\n"
                     "mov $0x277, %%rcx\n"
                     "rdmsr\n"
                     "and $0xFFFFFF00, %%edx\n"
                     "or $0x01, %%edx\n"
                     "wrmsr\n"
                     "wbinvd\n"
                     :
                     :
                     : "%rax", "%rcx", "%rdx");

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
        while (true) {
            __asm__ volatile("hlt");
        }
    }

    /* Load up all registers we need, and then load up the default SSE register configuration before
     * jumping. */
    __asm__ volatile("mov %0, %%rax\n"
                     "mov %1, %%rcx\n"
                     "mov %2, %%rsp\n"
                     "movl $0x1F80, (%%rsp)\n"
                     "ldmxcsr (%%rsp)\n"
                     "xor %%rdx, %%rdx\n"
                     "mov %%rdx, (%%rsp)\n"
                     "jmp *%%rax\n"
                     :
                     : "r"(EntryPoint), "r"(BootBlock), "r"((uint64_t)BootStack - 8)
                     : "%rax", "%rcx");

    /* It should be impossible to get here (because we jmp'ed instead of call'ing). */
    while (true) {
        __asm__ volatile("hlt");
    }
}
