/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_CONTEXT_INC_
#define _AMD64_CONTEXT_INC_

/* Sync these ones to their values in HalInterruptFrame (context.h). */
#define INTERRUPT_FRAME_RAX 0x00
#define INTERRUPT_FRAME_RCX 0x08
#define INTERRUPT_FRAME_RDX 0x10
#define INTERRUPT_FRAME_R8 0x18
#define INTERRUPT_FRAME_R9 0x20
#define INTERRUPT_FRAME_R10 0x28
#define INTERRUPT_FRAME_R11 0x30
#define INTERRUPT_FRAME_PADDING0 0x38
#define INTERRUPT_FRAME_XMM0 0x40
#define INTERRUPT_FRAME_XMM1 0x50
#define INTERRUPT_FRAME_XMM2 0x60
#define INTERRUPT_FRAME_XMM3 0x70
#define INTERRUPT_FRAME_XMM4 0x80
#define INTERRUPT_FRAME_XMM5 0x90
#define INTERRUPT_FRAME_PADDING1 0xA0
#define INTERRUPT_FRAME_MXCSR 0xA8
#define INTERRUPT_FRAME_IRQL 0xB0
#define INTERRUPT_FRAME_FAULT_ADDRESS 0xB8
#define INTERRUPT_FRAME_ERROR_CODE 0xC0
#define INTERRUPT_FRAME_INTERRUPT_NUMBER 0xC0
#define INTERRUPT_FRAME_RIP 0xC8
#define INTERRUPT_FRAME_SEG_CS 0xD0
#define INTERRUPT_FRAME_RFLAGS 0xD8
#define INTERRUPT_FRAME_RSP 0xE0
#define INTERRUPT_FRAME_SEG_SS 0xE8
#define INTERRUPT_FRAME_SIZE 0xF0

/* Sync these ones to their values in HalExceptionFrame (context.h). */
#define EXCEPTION_FRAME_RBX 0x00
#define EXCEPTION_FRAME_RBP 0x08
#define EXCEPTION_FRAME_RSI 0x10
#define EXCEPTION_FRAME_RDI 0x18
#define EXCEPTION_FRAME_R12 0x20
#define EXCEPTION_FRAME_R13 0x28
#define EXCEPTION_FRAME_R14 0x30
#define EXCEPTION_FRAME_R15 0x38
#define EXCEPTION_FRAME_XMM6 0x40
#define EXCEPTION_FRAME_XMM7 0x50
#define EXCEPTION_FRAME_XMM8 0x60
#define EXCEPTION_FRAME_XMM9 0x70
#define EXCEPTION_FRAME_XMM10 0x80
#define EXCEPTION_FRAME_XMM11 0x90
#define EXCEPTION_FRAME_XMM12 0xA0
#define EXCEPTION_FRAME_XMM13 0xB0
#define EXCEPTION_FRAME_XMM14 0xC0
#define EXCEPTION_FRAME_XMM15 0xD0
#define EXCEPTION_FRAME_MXCSR 0xE0
#define EXCEPTION_FRAME_RETURN_ADDRESS 0xE8
#define EXCEPTION_FRAME_SIZE 0xF0

/* Flags for the ENTER_INTERRUPT macro. */
#define INTERRUPT_FLAGS_HAS_ERROR_CODE 0x01

/* https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
 * Reference for the default x64 calling convention on PE files. */
.macro ENTER_INTERRUPT Flags_
    /* Make sure the Flags parameter is valid (or give it a default value). */
    .ifb Flags_
        .set Flags, 0
    .else
        .set Flags, \Flags_
    .endif

    /* Inform the compiler of the machine frame, and reserve the interrupt frame space. */
    .if (Flags & INTERRUPT_FLAGS_HAS_ERROR_CODE)
        .seh_pushframe @code
        sub $INTERRUPT_FRAME_SIZE - 48, %rsp
        .seh_stackalloc INTERRUPT_FRAME_SIZE - 48
    .else
        .seh_pushframe
        sub $INTERRUPT_FRAME_SIZE - 40, %rsp
        .seh_stackalloc INTERRUPT_FRAME_SIZE - 40
    .endif

    .seh_endprologue

    /* Save the volatile integer registers. */
    mov %rax, INTERRUPT_FRAME_RAX(%rsp)
    mov %rcx, INTERRUPT_FRAME_RCX(%rsp)
    mov %rdx, INTERRUPT_FRAME_RDX(%rsp)
    mov %r8, INTERRUPT_FRAME_R8(%rsp)
    mov %r9, INTERRUPT_FRAME_R9(%rsp)
    mov %r10, INTERRUPT_FRAME_R10(%rsp)
    mov %r11, INTERRUPT_FRAME_R11(%rsp)

    /* Save the volatile SSE registers. */
    movdqa %xmm0, INTERRUPT_FRAME_XMM0(%rsp)
    movdqa %xmm1, INTERRUPT_FRAME_XMM1(%rsp)
    movdqa %xmm2, INTERRUPT_FRAME_XMM2(%rsp)
    movdqa %xmm3, INTERRUPT_FRAME_XMM3(%rsp)
    movdqa %xmm4, INTERRUPT_FRAME_XMM4(%rsp)
    movdqa %xmm5, INTERRUPT_FRAME_XMM5(%rsp)

    /* Save the SSE configuration register. */
    stmxcsr INTERRUPT_FRAME_MXCSR(%rsp)

    /* Save the faulting address. */
    mov %cr2, %rax
    mov %rax, INTERRUPT_FRAME_FAULT_ADDRESS(%rsp)

    /* Save the current IRQL. */
    mov %cr8, %rax
    mov %rax, INTERRUPT_FRAME_IRQL(%rsp)

    /* iretq should restore RFLAGS later. */
    cld
.endm

.macro LEAVE_INTERRUPT
    /* Restore the previous IRQL. */
    mov INTERRUPT_FRAME_IRQL(%rsp), %rax
    mov %rax, %cr8

    /* Restore the SSE configuration register. */
    ldmxcsr INTERRUPT_FRAME_MXCSR(%rsp)

    /* Restore the volatile SSE registers. */
    movdqa INTERRUPT_FRAME_XMM0(%rsp), %xmm0
    movdqa INTERRUPT_FRAME_XMM1(%rsp), %xmm1
    movdqa INTERRUPT_FRAME_XMM2(%rsp), %xmm2
    movdqa INTERRUPT_FRAME_XMM3(%rsp), %xmm3
    movdqa INTERRUPT_FRAME_XMM4(%rsp), %xmm4
    movdqa INTERRUPT_FRAME_XMM5(%rsp), %xmm5

    /* Restore the volatile integer registers. */
    mov INTERRUPT_FRAME_RAX(%rsp), %rax
    mov INTERRUPT_FRAME_RCX(%rsp), %rcx
    mov INTERRUPT_FRAME_RDX(%rsp), %rdx
    mov INTERRUPT_FRAME_R8(%rsp), %r8
    mov INTERRUPT_FRAME_R9(%rsp), %r9
    mov INTERRUPT_FRAME_R10(%rsp), %r10
    mov INTERRUPT_FRAME_R11(%rsp), %r11

    /* Rollback the interrupt frame space, and return from the interrupt. */
    add $INTERRUPT_FRAME_RIP, %rsp
    iretq
.endm

.macro ENTER_EXCEPTION
    /* Reserve the exception frame space. */
    sub $EXCEPTION_FRAME_SIZE - 8, %rsp
    .seh_stackalloc EXCEPTION_FRAME_SIZE - 8

    /* Save the non-volatile integer registers. */
    mov %rbx, EXCEPTION_FRAME_RBX(%rsp)
    .seh_savereg %rbx, EXCEPTION_FRAME_RBX
    mov %rbp, EXCEPTION_FRAME_RBP(%rsp)
    .seh_savereg %rbp, EXCEPTION_FRAME_RBP
    mov %rsi, EXCEPTION_FRAME_RSI(%rsp)
    .seh_savereg %rsi, EXCEPTION_FRAME_RSI
    mov %rdi, EXCEPTION_FRAME_RDI(%rsp)
    .seh_savereg %rdi, EXCEPTION_FRAME_RDI
    mov %r12, EXCEPTION_FRAME_R12(%rsp)
    .seh_savereg %r12, EXCEPTION_FRAME_R12
    mov %r13, EXCEPTION_FRAME_R13(%rsp)
    .seh_savereg %r13, EXCEPTION_FRAME_R13
    mov %r14, EXCEPTION_FRAME_R14(%rsp)
    .seh_savereg %r14, EXCEPTION_FRAME_R14
    mov %r15, EXCEPTION_FRAME_R15(%rsp)
    .seh_savereg %r15, EXCEPTION_FRAME_R15

    /* Save the non-volatile XMM registers. */
    movdqa %xmm6, EXCEPTION_FRAME_XMM6(%rsp)
    .seh_savexmm %xmm6, EXCEPTION_FRAME_XMM6
    movdqa %xmm7, EXCEPTION_FRAME_XMM7(%rsp)
    .seh_savexmm %xmm7, EXCEPTION_FRAME_XMM7
    movdqa %xmm8, EXCEPTION_FRAME_XMM8(%rsp)
    .seh_savexmm %xmm8, EXCEPTION_FRAME_XMM8
    movdqa %xmm9, EXCEPTION_FRAME_XMM9(%rsp)
    .seh_savexmm %xmm9, EXCEPTION_FRAME_XMM9
    movdqa %xmm10, EXCEPTION_FRAME_XMM10(%rsp)
    .seh_savexmm %xmm10, EXCEPTION_FRAME_XMM10
    movdqa %xmm11, EXCEPTION_FRAME_XMM11(%rsp)
    .seh_savexmm %xmm11, EXCEPTION_FRAME_XMM11
    movdqa %xmm12, EXCEPTION_FRAME_XMM12(%rsp)
    .seh_savexmm %xmm12, EXCEPTION_FRAME_XMM12
    movdqa %xmm13, EXCEPTION_FRAME_XMM13(%rsp)
    .seh_savexmm %xmm13, EXCEPTION_FRAME_XMM13
    movdqa %xmm14, EXCEPTION_FRAME_XMM14(%rsp)
    .seh_savexmm %xmm14, EXCEPTION_FRAME_XMM14
    movdqa %xmm15, EXCEPTION_FRAME_XMM15(%rsp)
    .seh_savexmm %xmm15, EXCEPTION_FRAME_XMM15

    .seh_endprologue

    /* Save the SSE configuration register. */
    stmxcsr EXCEPTION_FRAME_MXCSR(%rsp)
.endm

.macro LEAVE_EXCEPTION
    /* Restore the SSE configuration register. */
    ldmxcsr EXCEPTION_FRAME_MXCSR(%rsp)

    /* Restore the non-volatile integer registers. */
    mov EXCEPTION_FRAME_RBX(%rsp), %rbx
    mov EXCEPTION_FRAME_RBP(%rsp), %rbp
    mov EXCEPTION_FRAME_RSI(%rsp), %rsi
    mov EXCEPTION_FRAME_RDI(%rsp), %rdi
    mov EXCEPTION_FRAME_R12(%rsp), %r12
    mov EXCEPTION_FRAME_R13(%rsp), %r13
    mov EXCEPTION_FRAME_R14(%rsp), %r14
    mov EXCEPTION_FRAME_R15(%rsp), %r15

    /* Restore the non-volatile XMM registers. */
    movdqa EXCEPTION_FRAME_XMM6(%rsp), %xmm6
    movdqa EXCEPTION_FRAME_XMM7(%rsp), %xmm7
    movdqa EXCEPTION_FRAME_XMM8(%rsp), %xmm8
    movdqa EXCEPTION_FRAME_XMM9(%rsp), %xmm9
    movdqa EXCEPTION_FRAME_XMM10(%rsp), %xmm10
    movdqa EXCEPTION_FRAME_XMM11(%rsp), %xmm11
    movdqa EXCEPTION_FRAME_XMM12(%rsp), %xmm12
    movdqa EXCEPTION_FRAME_XMM13(%rsp), %xmm13
    movdqa EXCEPTION_FRAME_XMM14(%rsp), %xmm14
    movdqa EXCEPTION_FRAME_XMM15(%rsp), %xmm15

    /* Rollback the exception frame space, and return. */
    add $EXCEPTION_FRAME_SIZE - 8, %rsp
    ret
.endm

#endif /* _AMD64_CONTEXT_INC_ */
