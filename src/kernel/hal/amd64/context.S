/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

#include <kernel/detail/amd64/context.inc>
#include <kernel/detail/amd64/irql.inc>

.extern HalpUpdateTss

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point for any new thread.
 *
 * PARAMETERS:
 *     (%rsp) StartFrame - Data about what function to transfer execution into.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
.seh_proc HalpThreadEntry
.global HalpThreadEntry
HalpThreadEntry:
    /* There should be a HalStartFrame in the stack at the moment. Let's make SEH treat it like a
     * stack allocation. */
    .seh_stackalloc START_FRAME_SIZE
    .seh_endprologue

    /* Drop the IRQL to passive/normal. */
    mov $KE_IRQL_PASSIVE, %rax
    mov %rax, %cr8

    /* Clean up our volatile integer registers. */
    xor %rax, %rax
    xor %rcx, %rcx
    xor %rdx, %rdx
    xor %r8, %r8
    xor %r9, %r9
    xor %r10, %r10
    xor %r11, %r11

    /* Clean up the volatile SSE registers. */
    pxor %xmm0, %xmm0
    pxor %xmm1, %xmm1
    pxor %xmm2, %xmm2
    pxor %xmm3, %xmm3
    pxor %xmm4, %xmm4
    pxor %xmm5, %xmm5

    /* Grab the entry point (and the start parameter). */
    mov START_FRAME_ENTRY_POINT(%rsp), %rax
    mov START_FRAME_PARAMETER(%rsp), %rcx
    add $START_FRAME_SIZE, %rsp

    /* Now we should be aligned at a 16-byte boundary; C functions expect a CALL instead of a jump
     * at this point, so we need to adjust the stack. */
    sub $8, %rsp
    jmp *%rax
.seh_endproc

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function transfers execution to the given thread context (by swapping our stack with
 *     theirs).
 *
 * PARAMETERS:
 *     (%rcx) CurrentContext - Current thread context pointer.
 *     (%rdx) TargetContext - Target thread context pointer.
 *
 * RETURN VALUE:
 *     We return to the context of the given thread; Execution can be returned to the current/old
 *     context by calling us again.
 *-----------------------------------------------------------------------------------------------*/
.seh_proc HalpSwitchContext
.global HalpSwitchContext
HalpSwitchContext:
    ENTER_EXCEPTION

    /* Skip saving the current stack on scheduler initialization. */
    test %rcx, %rcx
    jz 1f

    /* Save the current thread's stack pointer and notify other processors that we did so. */
    mov %rsp, CONTEXT_FRAME_RSP(%rcx)
    movq $0, CONTEXT_FRAME_BUSY(%rcx)

    /* And now we can wait until our any other processor that maybe still in the new thread exits
     * it, and setup the new stack. */
1:  mov CONTEXT_FRAME_BUSY(%rdx), %rax
    test %rax, %rax
    jz 3f
2:  pause
    mov CONTEXT_FRAME_BUSY(%rdx), %rax
    test %rax, %rax
    jnz 2b
3:  mov CONTEXT_FRAME_RSP(%rdx), %rsp
    call HalpUpdateTss

    LEAVE_EXCEPTION
.seh_endproc
