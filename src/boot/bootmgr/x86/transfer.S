/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function puts the processor in long mode, and transfers execution away to the kernel.
 *
 * PARAMETERS:
 *     Pml4 (esp + 4) - Physical address of the PML4 structure.
 *     BootData (esp + 8) - Virtual address (64-bits) where the loader boot data is located.
 *     EntryPoint (esp + 16) - Virtual address (64-bits) of the kernel entry point.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
.global _BiTransferExecution
_BiTransferExecution:
    /* Enable long mode and no-execute (for W^X) in the EFER. */
    mov $0xC0000080, %ecx
    rdmsr
    or $0x900, %eax
    wrmsr

    /* Enable PAE (physical address extension); Required for long mode. */
    mov %cr4, %eax
    or $0x20, %eax
    mov %eax, %cr4

    /* Enable the paging flag; CR3 is set first, otherwise we'll page fault trying to resolve the
     * current address. */
    mov 4(%esp), %eax
    mov %eax, %cr3
    mov %cr0, %eax
    or $0x80010000, %eax
    mov %eax, %cr0

    /* All we've left is loading a proper 64-bits GDT, and long ret'ing into 64-bits mode. */
    lgdt .LGdtSize
    mov $.LFinish, %eax
    push $0x08
    push %eax
    lret

.code64
.LFinish:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Skip the return address and Pml4, so that we can grab the entry point + boot data. */
    add $8, %rsp
    pop %rcx
    pop %rax

    /* Setup a proper aligned stack (16-bytes alignemnt), and jump execution. */
    mov $0x7C00, %rsp
    jmp *%rax

.align 8
.LGdtDescs: .quad 0x0000000000000000, 0x00AF9A000000FFFF, 0x00AF92000000FFFF
.LGdtSize: .short .LGdtSize - .LGdtDescs - 1
.LGdtBase: .long .LGdtDescs, 0
