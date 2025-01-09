/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

.extern HalpOnlineProcessorCount
.extern HalpProcessorList

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

    /* Setup a proper 32-bits GDT. */
    lgdtl (HalpTemporaryGdtSize32 - HalpApEntry + 0x8000)

    /* Set the protected bit, and ljmp into protected mode. */
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
    ljmpl $8, $(0f - HalpApEntry + 0x8000)

.code32
0:
    /* We just loaded the new GDT, so we need to fix our segment registers. */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* We should always be at 0x8000 no matter what; Save it up in ebp, for when we want to
     * access the kernel page map. */
    mov $0x8000, %ebp

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
    mov (HalpKernelPageMap - HalpApEntry)(%ebp), %eax
    mov %eax, %cr3
    mov %cr0, %eax
    or $0x80010000, %eax
    mov %eax, %cr0

    /* All we've left is loading a proper 64-bits GDT, and long jmp'ing into 64-bits mode. */
    lgdt (HalpTemporaryGdtSize64 - HalpApEntry + 0x8000)
    ljmpl $8, $(0f - HalpApEntry + 0x8000)

.code64
0:
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* Get our kernel processor ID, and indicate that we're fully online (and about to enter
     * KiSystemStartup). */
    movabs $HalpOnlineProcessorCount, %rbp
    mov $1, %eax
    lock xadd %eax, (%rbp)

    /* Now we only need to setup the stack, and we can get out of assembly land. */
    movabs $HalpProcessorList, %rbp
    mov (%rbp), %rbp
    mov (%rbp, %rax, 8), %rdx
    lea 0x3000(%rdx), %rsp
    movabs $KiSystemStartup, %rax
    xor %rcx, %rcx
    jmp *%rax

.align 8
HalpTemporaryGdtDescs32: .quad 0x0000000000000000, 0x00CF9A000000FFFF, 0x00CF92000000FFFF
HalpTemporaryGdtSize32: .short HalpTemporaryGdtSize32 - HalpTemporaryGdtDescs32 - 1
HalpTemporaryGdtBase32: .long HalpTemporaryGdtDescs32 - HalpApEntry + 0x8000

.align 8
HalpTemporaryGdtDescs64: .quad 0x0000000000000000, 0x00AF9A000000FFFF, 0x00AF92000000FFFF
HalpTemporaryGdtSize64: .short HalpTemporaryGdtSize64 - HalpTemporaryGdtDescs64 - 1
HalpTemporaryGdtBase64: .long HalpTemporaryGdtDescs64 - HalpApEntry + 0x8000, 0

.global HalpKernelPageMap
HalpKernelPageMap: .quad 0
