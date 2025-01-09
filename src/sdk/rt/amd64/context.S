/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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
 .seh_proc RtSaveContext
.global RtSaveContext
RtSaveContext:
    /* Only way to get the flags is by pushfq'ing them, warn the SEH we're doing that. */ 
    pushfq
    .seh_stackalloc 8
    .seh_endprologue

    /* RCX itself has already been lost on fn call (no way around that), but we can pretty much
       capture everything else. */
    mov %rax, (%rcx)
    mov %rcx, 8(%rcx)
    mov %rdx, 16(%rcx)
    mov %rbx, 24(%rcx)
    mov %rbp, 40(%rcx)
    mov %rsi, 48(%rcx)
    mov %rdi, 56(%rcx)
    mov %r8, 64(%rcx)
    mov %r9, 72(%rcx)
    mov %r10, 80(%rcx)
    mov %r11, 88(%rcx)
    mov %r12, 96(%rcx)
    mov %r13, 104(%rcx)
    mov %r14, 112(%rcx)
    mov %r15, 120(%rcx)

    /* The return address is the "current" PC (in the caller's POV). */
    mov 8(%rsp), %rax
    mov %rax, 128(%rcx)

    /* Pop RFLAGS back, and save it. */
    mov (%rsp), %rax
    mov %rax, 136(%rcx)

    /* Load up the caller's RSP, and save it. */
    lea 16(%rsp), %rax
    mov %rax, 32(%rcx)

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
    mov (%rcx), %r8
    mov 32(%rcx), %rax
    mov 128(%rcx), %rbx
    mov 136(%rcx), %rdx
    mov %rbx, -8(%rax)
    mov %rdx, -16(%rax)
    mov %r8, -24(%rax)

    /* Restore GPRs (except RCX and RAX). */
    mov 16(%rcx), %rdx
    mov 24(%rcx), %rbx
    mov 40(%rcx), %rbp
    mov 48(%rcx), %rsi
    mov 56(%rcx), %rdi
    mov 64(%rcx), %r8
    mov 72(%rcx), %r9
    mov 80(%rcx), %r10
    mov 88(%rcx), %r11
    mov 96(%rcx), %r12
    mov 104(%rcx), %r13
    mov 112(%rcx), %r14
    mov 120(%rcx), %r15

    /* Restore RCX, and RSP. */
    mov 8(%rcx), %rcx
    lea -24(%rax), %rsp

    /* HAL functions can only be called by the kernel (this is never gonna be cross rings!), wrap
       up by restore RAX, RFLAGS, and ret'ing into the target RIP. */
    pop %rax
    popfq
    ret
.seh_endproc
