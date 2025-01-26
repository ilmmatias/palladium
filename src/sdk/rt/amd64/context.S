/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/* Keep this in sync with the RtContext struct in include/rt/amd64/context.h. */
#define CONTEXT_RAX 0x00
#define CONTEXT_RCX 0x08
#define CONTEXT_RDX 0x10
#define CONTEXT_RBX 0x18
#define CONTEXT_RSP 0x20
#define CONTEXT_RBP 0x28
#define CONTEXT_RSI 0x30
#define CONTEXT_RDI 0x38
#define CONTEXT_R8 0x40
#define CONTEXT_R9 0x48
#define CONTEXT_R10 0x50
#define CONTEXT_R11 0x58
#define CONTEXT_R12 0x60
#define CONTEXT_R13 0x68
#define CONTEXT_R14 0x70
#define CONTEXT_R15 0x78
#define CONTEXT_XMM0 0x80
#define CONTEXT_XMM1 0x90
#define CONTEXT_XMM2 0xA0
#define CONTEXT_XMM3 0xB0
#define CONTEXT_XMM4 0xC0
#define CONTEXT_XMM5 0xD0
#define CONTEXT_XMM6 0xE0
#define CONTEXT_XMM7 0xF0
#define CONTEXT_XMM8 0x100
#define CONTEXT_XMM9 0x110
#define CONTEXT_XMM10 0x120
#define CONTEXT_XMM11 0x130
#define CONTEXT_XMM12 0x140
#define CONTEXT_XMM13 0x150
#define CONTEXT_XMM14 0x160
#define CONTEXT_XMM15 0x170
#define CONTEXT_RIP 0x180
#define CONTEXT_RFLAGS 0x188

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the current processor state/context.
 *
 * PARAMETERS:
 *     Context - Output; Where to put the register state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
 .seh_proc RtSaveContext
.global RtSaveContext
RtSaveContext:
    /* Only way to get the flags is by pushfq'ing them, warn the compiler we're doing that. */ 
    pushfq
    .seh_stackalloc 8
    .seh_endprologue

    /* Save the integer GPR. */
    mov %rax, CONTEXT_RAX(%rcx)
    mov %rcx, CONTEXT_RCX(%rcx)
    mov %rdx, CONTEXT_RDX(%rcx)
    mov %rbx, CONTEXT_RBX(%rcx)
    mov %rbp, CONTEXT_RBP(%rcx)
    mov %rsi, CONTEXT_RSI(%rcx)
    mov %rdi, CONTEXT_RDI(%rcx)
    mov %r8, CONTEXT_R8(%rcx)
    mov %r9, CONTEXT_R9(%rcx)
    mov %r10, CONTEXT_R10(%rcx)
    mov %r11, CONTEXT_R11(%rcx)
    mov %r12, CONTEXT_R12(%rcx)
    mov %r13, CONTEXT_R13(%rcx)
    mov %r14, CONTEXT_R14(%rcx)
    mov %r15, CONTEXT_R15(%rcx)

    /* Save the XMM registers. */
    movdqa %xmm0, CONTEXT_XMM0(%rcx)
    movdqa %xmm1, CONTEXT_XMM1(%rcx)
    movdqa %xmm2, CONTEXT_XMM2(%rcx)
    movdqa %xmm3, CONTEXT_XMM3(%rcx)
    movdqa %xmm4, CONTEXT_XMM4(%rcx)
    movdqa %xmm5, CONTEXT_XMM5(%rcx)
    movdqa %xmm6, CONTEXT_XMM6(%rcx)
    movdqa %xmm7, CONTEXT_XMM7(%rcx)
    movdqa %xmm8, CONTEXT_XMM8(%rcx)
    movdqa %xmm9, CONTEXT_XMM9(%rcx)
    movdqa %xmm10, CONTEXT_XMM10(%rcx)
    movdqa %xmm11, CONTEXT_XMM11(%rcx)
    movdqa %xmm12, CONTEXT_XMM12(%rcx)
    movdqa %xmm13, CONTEXT_XMM13(%rcx)
    movdqa %xmm14, CONTEXT_XMM14(%rcx)
    movdqa %xmm15, CONTEXT_XMM15(%rcx)

    /* The return address is the "current" PC (in the caller's POV). */
    mov 8(%rsp), %rax
    mov %rax, CONTEXT_RIP(%rcx)

    /* Pop RFLAGS back, and save it. */
    mov (%rsp), %rax
    mov %rax, CONTEXT_RFLAGS(%rcx)

    /* Load up the caller's RSP, and save it. */
    lea 16(%rsp), %rax
    mov %rax, CONTEXT_RSP(%rcx)

    /* We're done, cleanup the stack and return to the caller. */
    add $8, %rsp
    ret
.seh_endproc

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores the current processor state/context from whichever state is given.
 *
 * PARAMETERS:
 *     Context - The register state to be restore.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
 .seh_proc RtRestoreContext
.global RtRestoreContext
RtRestoreContext:
    .seh_endprologue

    /* Save up the target RSP into somewhere we can use it. We need to push RFLAGS (and RAX, which
       we're using to store the target RSP) into it to restore after we switch stacks. */
    mov CONTEXT_RAX(%rcx), %r8
    mov CONTEXT_RSP(%rcx), %rax
    mov CONTEXT_RIP(%rcx), %rbx
    mov CONTEXT_RFLAGS(%rcx), %rdx
    mov %rbx, -8(%rax)
    mov %rdx, -16(%rax)
    mov %r8, -24(%rax)

    /* Restore GPRs (except RCX and RAX). */
    mov CONTEXT_RDX(%rcx), %rdx
    mov CONTEXT_RBX(%rcx), %rbx
    mov CONTEXT_RBP(%rcx), %rbp
    mov CONTEXT_RSI(%rcx), %rsi
    mov CONTEXT_RDI(%rcx), %rdi
    mov CONTEXT_R8(%rcx), %r8
    mov CONTEXT_R9(%rcx), %r9
    mov CONTEXT_R10(%rcx), %r10
    mov CONTEXT_R11(%rcx), %r11
    mov CONTEXT_R12(%rcx), %r12
    mov CONTEXT_R13(%rcx), %r13
    mov CONTEXT_R14(%rcx), %r14
    mov CONTEXT_R15(%rcx), %r15

    /* Restore the XMM registers. */
    movdqa CONTEXT_XMM0(%rcx), %xmm0
    movdqa CONTEXT_XMM1(%rcx), %xmm1
    movdqa CONTEXT_XMM2(%rcx), %xmm2
    movdqa CONTEXT_XMM3(%rcx), %xmm3
    movdqa CONTEXT_XMM4(%rcx), %xmm4
    movdqa CONTEXT_XMM5(%rcx), %xmm5
    movdqa CONTEXT_XMM6(%rcx), %xmm6
    movdqa CONTEXT_XMM7(%rcx), %xmm7
    movdqa CONTEXT_XMM8(%rcx), %xmm8
    movdqa CONTEXT_XMM9(%rcx), %xmm9
    movdqa CONTEXT_XMM10(%rcx), %xmm10
    movdqa CONTEXT_XMM11(%rcx), %xmm11
    movdqa CONTEXT_XMM12(%rcx), %xmm12
    movdqa CONTEXT_XMM13(%rcx), %xmm13
    movdqa CONTEXT_XMM14(%rcx), %xmm14
    movdqa CONTEXT_XMM15(%rcx), %xmm15

    /* Restore RCX, and RSP. */
    mov CONTEXT_RCX(%rcx), %rcx
    lea -24(%rax), %rsp

    /* HAL functions can only be called by the kernel (this is never gonna be cross rings!), wrap
       up by restore RAX, RFLAGS, and ret'ing into the target RIP. */
    pop %rax
    popfq
    ret
.seh_endproc
