/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

.text

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
 .seh_proc HalSaveContext
.global HalSaveContext
HalSaveContext:
    /* Only way to get the flags is by pushfq'ing them, warn the SEH we're doing that. */ 
    pushfq
    .seh_stackalloc 8
    .seh_endprologue

    /* RCX itself has already been lost on fn call (no way around that), but we can pretty much
       capture everything else. */
    mov %r15, (%rcx)
    mov %r14, 8(%rcx)
    mov %r13, 16(%rcx)
    mov %r12, 24(%rcx)
    mov %r11, 32(%rcx)
    mov %r10, 40(%rcx)
    mov %r9, 48(%rcx)
    mov %r8, 56(%rcx)
    mov %rdi, 64(%rcx)
    mov %rsi, 72(%rcx)
    mov %rbp, 80(%rcx)
    mov %rbx, 88(%rcx)
    mov %rdx, 96(%rcx)
    mov %rcx, 104(%rcx)
    mov %rax, 112(%rcx)

    xor %rax, %rax
    mov %rax, 120(%rcx)
    mov %rax, 128(%rcx)

    /* The return address is the "current" PC (in the caller's POV). */
    mov 8(%rsp), %rax
    mov %rax, 136(%rcx)

    /* We're assuming kernel mode (user mode can't call HAL functions after all). */
    mov $0x08, %rax
    mov %rax, 144(%rcx)

    /* Pop RFLAGS back, and save it. */
    mov (%rsp), %rax
    mov %rax, 152(%rcx)

    /* Load up the caller's RSP, and save it. */
    lea 16(%rsp), %rax
    mov %rax, 160(%rcx)

    /* Standard stack segment for the kernel. */
    mov $0x10, %rax
    mov %rax, 168(%rcx)

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
 .seh_proc HalRestoreContext
.global HalRestoreContext
HalRestoreContext:
    .seh_endprologue

    /* Save up the target RSP into somewhere we can use it. We need to push RFLAGS (and RAX, which
       we're using to store the target RSP) into it to restore after we switch stacks. */
    mov 160(%rcx), %rax
    mov 152(%rcx), %rbx
    mov 136(%rcx), %rdx
    mov 112(%rcx), %r8
    mov %rdx, -8(%rax)
    mov %rbx, -16(%rax)
    mov %r8, -24(%rax)

    /* Restore GPRs (except RCX and RAX). */
    mov 96(%rcx), %rdx
    mov 88(%rcx), %rbx
    mov 80(%rcx), %rbp
    mov 72(%rcx), %rsi
    mov 64(%rcx), %rdi
    mov 56(%rcx), %r8
    mov 48(%rcx), %r9
    mov 40(%rcx), %r10
    mov 32(%rcx), %r11
    mov 24(%rcx), %r12
    mov 16(%rcx), %r13
    mov 8(%rcx), %r14
    mov (%rcx), %r15

    /* Restore RCX, and RSP. */
    mov 104(%rcx), %rcx
    lea -24(%rax), %rsp

    /* HAL functions can only be called by the kernel (this is never gonna be cross rings!), wrap
       up by restore RAX, RFLAGS, and ret'ing into the target RIP. */
    pop %rax
    popfq
    ret
.seh_endproc
