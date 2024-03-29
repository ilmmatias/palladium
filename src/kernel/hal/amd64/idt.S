/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.altmacro
.text

.extern HalpInterruptHandler

.macro SAVE_REGS
    push %rax
    .seh_pushreg %rax
    push %rcx
    .seh_pushreg %rcx
    push %rdx
    .seh_pushreg %rdx
    push %rbx
    .seh_pushreg %rbx
    push %rbp
    .seh_pushreg %rbp
    push %rsi
    .seh_pushreg %rsi
    push %rdi
    .seh_pushreg %rdi
    push %r8
    .seh_pushreg %r8
    push %r9
    .seh_pushreg %r9
    push %r10
    .seh_pushreg %r10
    push %r11
    .seh_pushreg %r11
    push %r12
    .seh_pushreg %r12
    push %r13
    .seh_pushreg %r13
    push %r14
    .seh_pushreg %r14
    push %r15
    .seh_pushreg %r15
.endm

.macro RESTORE_REGS
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %r11
    pop %r10
    pop %r9
    pop %r8
    pop %rdi
    pop %rsi
    pop %rbp
    pop %rbx
    pop %rdx
    pop %rcx
    pop %rax
.endm

.macro NO_ERRCODE num
.seh_proc HalpInterruptHandler\num
HalpInterruptHandler\num:
    .seh_pushframe
    push $0
    push $\num
    .seh_stackalloc 16
    SAVE_REGS
    .seh_endprologue
    mov %rsp, %rcx
    cld
    call HalpInterruptHandler
    RESTORE_REGS
    add $16, %rsp
    iretq
.seh_endproc
.endm

.macro ERRCODE num
.seh_proc HalpInterruptHandler\num
HalpInterruptHandler\num:
    .seh_pushframe @code
    push $\num
    .seh_stackalloc 8
    SAVE_REGS
    .seh_endprologue
    mov %rsp, %rcx
    cld
    call HalpInterruptHandler
    RESTORE_REGS
    add $16, %rsp
    iretq
.seh_endproc
.endm

.macro ENTRY num
    .quad HalpInterruptHandler\num
.endm

/* Some of the exceptions (in interrupts 0-31) have error codes (which we use ERRCODE instead
   of NO_ERRCODE to handle), so we need to manually define them. */

NO_ERRCODE 0
NO_ERRCODE 1
NO_ERRCODE 2
NO_ERRCODE 3
NO_ERRCODE 4
NO_ERRCODE 5
NO_ERRCODE 6
NO_ERRCODE 7
ERRCODE 8
NO_ERRCODE 9
ERRCODE 10
ERRCODE 11
ERRCODE 12
ERRCODE 13
ERRCODE 14
NO_ERRCODE 15
NO_ERRCODE 16
ERRCODE 17
NO_ERRCODE 18
NO_ERRCODE 19
NO_ERRCODE 20
ERRCODE 21
NO_ERRCODE 22
NO_ERRCODE 23
NO_ERRCODE 24
NO_ERRCODE 25
NO_ERRCODE 26
NO_ERRCODE 27
NO_ERRCODE 28
NO_ERRCODE 29
ERRCODE 30
NO_ERRCODE 31

.set i, 32
.rept 224
    NO_ERRCODE %i
    .set i, i + 1
.endr

.section .rodata

.global HalpInterruptHandlerTable
HalpInterruptHandlerTable:
.set i, 0
.rept 256
    ENTRY %i
    .set i, i + 1
.endr
