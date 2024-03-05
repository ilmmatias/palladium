/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point for AP cores (it gets called after we send the SIPI).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
.global HalpApEntry
.code16
HalpApEntry:
    cli
    cld

    /* Temporary stack for retf. */
    mov %cs, %bx
    shl $4, %bx
    lea HalpTemporaryApStack - HalpApEntry(%bx), %sp

    /* Setup a proper 32-bits GDT. */
    lea HalpTemporaryGdtSize32 - HalpApEntry(%bx), %eax
    lgdtl (%eax)

    /* Set the protected bit, and ljmp into protected mode. */
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0

    pushl $8
    lea 0f - HalpApEntry(%bx), %eax
    push %eax
    lretl

.code32
0:
    /* We just loaded the new GDT, so we need to fix our segment registers. */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Enable long mode and no-execute (for W^X) in the EFER. */
    mov $0xC0000080, %ecx
    rdmsr
    or $0x900, %eax
    wrmsr

    /* Enable PAE (physical address extension) + PSE (support for larger pages);
     * Required for long mode + the processor not crashing with our page tables. */
    mov %cr4, %eax
    or $0x668, %eax
    mov %eax, %cr4

    /* Setup our WC PAT entry. */
    mov $0x277, %ecx
    rdmsr
    and $0xFFFFFF00, %edx
    or $0x01, %edx
    wrmsr

    /* Install the kernel page map, and enable paging. */
    mov HalpKernelPageMap - HalpApEntry(%bx), %eax
    mov %eax, %cr3
    mov %cr0, %eax
    or $0x80010000, %eax
    mov %eax, %cr0

    /* All we've left is loading a proper 64-bits GDT, and long jmp'ing into 64-bits mode. */
    lea HalpTemporaryGdtSize64 - HalpApEntry(%bx), %eax
    lgdtl (%eax)
    lea 0f - HalpApEntry(%bx), %eax
    push $0x08
    push %eax
    lretl

.code64
0:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Setup the stack, indicate we're only, and finally get out of assembly (and low address)
       land. */
    mov HalpApStructure - HalpApEntry(%ebx), %rdx
    lea 0x3000(%rdx), %rsp
    lock incb 0x10(%rdx)
    movabs $KiSystemStartup, %rax
    xor %rcx, %rcx
    jmp *%rax

.fill 128, 1, 0
HalpTemporaryApStack:

.align 8
HalpTemporaryGdtDescs32: .quad 0x0000000000000000, 0x00CF9A000000FFFF, 0x00CF92000000FFFF
HalpTemporaryGdtSize32: .short HalpTemporaryGdtSize32 - HalpTemporaryGdtDescs32 - 1
HalpTemporaryGdtBase32: .long HalpTemporaryGdtDescs32 - HalpApEntry + 0x8000

HalpTemporaryGdtDescs64: .quad 0x0000000000000000, 0x00AF9A000000FFFF, 0x00AF92000000FFFF
HalpTemporaryGdtSize64: .short HalpTemporaryGdtSize64 - HalpTemporaryGdtDescs64 - 1
HalpTemporaryGdtBase64: .long HalpTemporaryGdtDescs64 - HalpApEntry + 0x8000, 0

.global HalpKernelPageMap
HalpKernelPageMap: .quad 0

.global HalpApStructure
HalpApStructure: .quad 0
