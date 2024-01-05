/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function drops the processor back to real mode, and executes a BIOS interrupt with the
 *     given processor context.
 *
 * PARAMETERS:
 *     Number (esp + 4) - Interrupt number.
 *     Registers (esp + 8) - I/O; State of the processor when executing the interrupt, and state
 *                           after.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
.global _BiosCall
_BiosCall:
    /* We should be in low memory, so start by overwriting the int call number up ahead and saving
     * the regs pointer. */
    mov 4(%esp), %al
    mov %al, (.LInt + 1)
    mov 8(%esp), %eax
    mov %eax, (.LInRegs)
    add $36, %eax
    mov %eax, (.LOutRegs)

    /* Worst case the BIOS might overwrite our GDT, so we need to save that. */
    sgdt .LSavedGdt

    /* We'll popa on real mode, trashing all registers. Save anything the compiler expects us to
     * not mess with. */
    push %ebx
    push %esi
    push %edi
    push %ebp

    /* startup.com should have setup our GDT properly, so drop back to protected 16-bits mode. */
    mov $0x08, %ax
    push %ax
    lea .LEnd, %eax
    push %ax
    lea .LSavedGdt, %eax
    push %ax
    xor %eax, %eax
    push %ax
    lea .LReal, %eax
    push %ax
    lea .LProtected16, %eax
    push $0x18
    push %eax
    lret

.code16
.LProtected16:
    /* We also need to load up the data segments. */
    mov $0x20, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    /* IDT should still be in place, disable the PE bit and drop to real mode. */
    mov %cr0, %eax
    and $0xFE, %al
    mov %eax, %cr0

    lret

.LReal:
    /* Zero out all the segments, and we should be ready to reenable interrupts and execute
     * what was requested. */
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    .byte 0x67, 0x89, 0x25 /* mov %sp, (.LSavedEsp) */
    .long .LSavedEsp
    .byte 0x67, 0x8B, 0x25 /* mov (.LInRegs), %sp */
    .long .LInRegs
    popal
    popfl
    .byte 0x67, 0x8B, 0x25 /* mov (.LSavedEsp), %sp */
    .long .LSavedEsp

    sti
.LInt:
    int $0
    cli

    .byte 0x67, 0x8B, 0x25 /* mov (.LOutRegs), %sp */
    .long .LOutRegs
    pushfl
    pushal
    .byte 0x67, 0x8B, 0x25 /* mov (.LSavedEsp), %sp */
    .long .LSavedEsp

    /* Restore the GDT and get right back onto pmode (no need to go to 16-bits pmode first). */
    pop %bx
    lgdt (%bx)

    mov %cr0, %eax
    or $1, %al
    mov %eax, %cr0

    lret

.code32
.LEnd:
    /* Restore the segments and registers, and give control back to the caller. */
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    pop %ebp
    pop %edi
    pop %esi
    pop %ebx

    ret

.align 8
.LSavedGdt: .quad 0
.LSavedEsp: .long 0
.LInRegs: .long 0
.LOutRegs: .long 0
