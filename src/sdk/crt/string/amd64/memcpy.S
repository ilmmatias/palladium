/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements optimized code to copy bytes from one region of memory to another.
 *
 * PARAMETERS:
 *     (%rcx) dest - Destination buffer.
 *     (%rdx) src - Source buffer.
 *     (%r8) count - How many bytes to copy.
 *
 * RETURN VALUE:
 *     (%rax) Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
.seh_proc memcpy
.global memcpy
.align 16
memcpy:
    push %r12
    .seh_pushreg %r12
    .seh_endprologue

    mov %rcx, %rax
    cmp $64, %r8
    jl .Lsmall16

    /* Our hot loop uses aligned SSE instructions, so let's try aligning the output pointer
     * beforehand. */
    movups (%rdx), %xmm0
    movups %xmm0, (%rcx)
    add $16, %rcx
    and $0x0FFFFFFFFFFFFFFF0, %rcx
    add %rcx, %rdx
    sub %rax, %rdx
    sub %rcx, %r8
    add %rax, %r8

    /* Break out if alignining the address made our size too small. */
    cmp $64, %r8
    jl .Lsmall16_nocmp

    /* Save the end point of the writes so that we can finish everything without any
     * branches (outside of the main loop). */
    lea -16(%rcx, %r8), %r9
    lea -48(%rcx, %r8), %r10
    and $0x0FFFFFFFFFFFFFFF0, %r10
    lea -16(%rdx, %r8), %r11
    sub %rcx, %r10
    lea (%rdx, %r10), %r12
    add %rcx, %r10
    shr $6, %r8

.align 16
.Lhot:
    /* Main hot loop, copy 64 bytes per iteration (with aligned instructions on the
     * destination). */
    movups (%rdx), %xmm0
    movups 16(%rdx), %xmm1
    movups 32(%rdx), %xmm2
    movups 48(%rdx), %xmm3
    movaps %xmm0, (%rcx) 
    movaps %xmm1, 16(%rcx)
    movaps %xmm2, 32(%rcx)
    movaps %xmm3, 48(%rcx)
    add $64, %rcx
    add $64, %rdx
    sub $1, %r8
    jnz .Lhot

.Ltrail64:
    /* We now have between 0-63 trailing bytes. Repeated/overlapping store operations are
     * less expensive than branches/branch mispredictions; We use 4 read ops+4 write ops in
     * total here so that we don't need any branch. */
    movups (%r12), %xmm0
    movups 16(%r12), %xmm1
    movups 32(%r12), %xmm2
    movups (%r11), %xmm3
    movaps %xmm0, (%r10)
    movaps %xmm1, 16(%r10)
    movaps %xmm2, 32(%r10)
    movups %xmm3, (%r9)
    pop %r12
    ret

.align 2
.Lsmall16:
    cmp $16, %r8
    jl .Lsmall4

.align 2
.Lsmall16_nocmp:
    /* We have between 16-63 bytes, use 4 possibly/probably overlapping writes to handle it. */
    lea -16(%rcx, %r8), %r9
    lea -16(%rdx, %r8), %r10
    and $0x20, %r8
    shr $1, %r8

    movups (%rdx), %xmm0
    movups (%r10), %xmm1
    movups (%rdx, %r8), %xmm2
    movups %xmm0, (%rcx)
    movups %xmm1, (%r9)
    movups %xmm2, (%rcx, %r8)
    neg %r8
    movups (%r10, %r8), %xmm3
    movups %xmm3, (%r9, %r8)

    pop %r12
    ret

.align 2
.Lsmall4:
    cmp $4, %r8
    jl .Lsmall3

    /* We have between 4-15 bytes, use same technique as above (but with 32-bit values). */
    lea -4(%rcx, %r8), %r9
    lea -4(%rdx, %r8), %r10
    and $0x08, %r8
    shr $1, %r8

    mov (%rdx), %r11d
    mov %r11d, (%rcx)
    mov (%r10), %r11d
    mov %r11d, (%r9)
    mov (%rdx, %r8), %r11d
    mov %r11d, (%rcx, %r8)
    neg %r8
    mov (%r10, %r8), %r11d
    mov %r11d, (%r9, %r8)

    pop %r12
    ret

.align 2
.Lsmall3:
    /* We have between 0-3 bytes, worst case (for us), write the first byte followed by the
     * last two. */

    test %r8, %r8
    jz .Ledata
    mov (%rdx), %r9b
    mov %r9b, (%rcx)

    cmp $2, %r8
    jl .Ledata
    mov -2(%rdx, %r8), %r9w
    mov %r9w, -2(%rcx, %r8)

.align 2
.Ledata:
    pop %r12
    ret
.seh_endproc
