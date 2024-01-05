/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function puts the processor in long mode, and transfers execution away to the kernel.
 *
 * PARAMETERS:
 *     Pml4 (esp + 4) - Physical address of the PML4 structure.
 *     BootData (esp + 8) - Virtual address (64-bits) where the loader boot data is located.
 *     EntryPoint (esp + 16) - Virtual address (64-bits) of the kernel entry point.
 *     StackTop (esp + 24) - Virtual address (64-bits) for the stack. It grows backwards on x86,
 *                           so remember to sum the size before passing it to us!
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
.global _BiJumpPalladium
_BiJumpPalladium:
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

    /* Welcome to long mode, we'll probably crash if we don't setup an IDT as soon as possible;
       Remap the PIC before enabling interrupts. */
    mov $0x11, %al
    outb %al, $0x20
    mov $0x11, %al
    outb %al, $0xA0

    mov $0x20, %al
    outb %al, $0x21
    mov $0x28, %al
    outb %al, $0xA1

    mov $0x04, %al
    outb %al, $0x21
    mov $0x02, %al
    outb %al, $0xA1

    /* Load the IDT, and enable interrupts. */ 
    lidt .LIdtSize
    sti

    /* Skip the return address and Pml4, so that we can grab the boot data and RIP+RSP. */
    add $8, %rsp
    pop %r8
    pop %rax
    pop %rdx
    lea 0x4000(%rdx), %rsp
    mov $1, %rcx
    jmp *%rax

.align 8
.LGdtDescs: .quad 0x0000000000000000, 0x00AF9A000000FFFF, 0x00AF92000000FFFF
.LGdtSize: .short .LGdtSize - .LGdtDescs - 1
.LGdtBase: .long .LGdtDescs, 0

.align 16
.global _IdtDescs
_IdtDescs: .fill 512, 8, 0

.LIdtSize: .short .LIdtSize - _IdtDescs - 1
.LIdtBase: .long _IdtDescs, 0

.macro irq_err num
IrqStub\num:
    pushq $\num
    jmp IrqStub
.endm

.macro irq_no_err num
IrqStub\num:
    pushq $0
    pushq $\num
    jmp IrqStub
.endm

IrqStub:
    cmpq $0x20, (%rsp)
    jae .LIrq
    0: jmp 0b

.LIrq:
    cmpq $0x28, (%rsp)
    jae .LSlave
    push %rax
    mov $0x20, %al
    outb %al, $0x20
    pop %rax
    jmp .LExit

.LSlave:
    push %rax
    mov $0x20, %al
    outb %al, $0x20
    mov $0x20, %al
    outb %al, $0xA0
    pop %rax

.LExit:
    add $16, %rsp
    iretq

/* The first 32 interrupts need special handling, as we want to hang if we encounter them
   (they're exceptions). */
irq_no_err 0
irq_no_err 1
irq_no_err 2
irq_no_err 3
irq_no_err 4
irq_no_err 5
irq_no_err 6
irq_no_err 7
irq_err 8
irq_no_err 9
irq_err 10
irq_err 11
irq_err 12
irq_err 13
irq_err 14
irq_no_err 15
irq_no_err 16
irq_err 17
irq_no_err 18
irq_no_err 19
irq_no_err 20
irq_no_err 21
irq_no_err 22
irq_no_err 23
irq_no_err 24
irq_no_err 25
irq_no_err 26
irq_no_err 27
irq_no_err 28
irq_no_err 29
irq_err 30
irq_no_err 31

/* Other than that, we don't care about anything else, so lets hardcode it as int $0x20. */
IrqStub32:
    pushq $0
    pushq 32
    jmp IrqStub

.global _IrqStubTable
_IrqStubTable:
.irp i,  0,  1,  2,  3,  4,  5 , 6 , 7, \
         8,  9, 10, 11, 12, 13, 14, 15, \
         16, 17, 18, 19, 20, 21, 22, 23, \
         24, 25, 26, 27, 28, 29, 30, 31
    .long IrqStub\i
.endr
    .long IrqStub32
