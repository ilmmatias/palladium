/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

.altmacro
.text

.macro NO_ERRCODE num
Handler\num:
    push $0
    push $\num
    jmp CommonStub
.endm

.macro ERRCODE num
Handler\num:
    push $\num
    jmp CommonStub
.endm

.macro ENTRY num
    .quad Handler\num
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

.extern HalpInterruptHandler
CommonStub:
    /* AMD64 has no PUSHA/POPA, so we're forced to save state manually.
       No XMM state is saved, as you shouldn't be using it in kernel mode; Only the thread
       scheduler saves it (on thread switch). */ 
	push %rax
    push %rcx
    push %rdx
    push %rbx
    push %rbp
    push %rsi
    push %rdi
    push %r8
    push %r9
    push %r10
    push %r11
    push %r12
    push %r13
    push %r14
    push %r15

    mov %rsp, %rcx
    call HalpInterruptHandler

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

    add $16, %rsp
    iretq

.section .rodata

.global HalpInterruptHandlerTable
HalpInterruptHandlerTable:
.set i, 0
.rept 256
    ENTRY %i
    .set i, i + 1
.endr
