/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

.text

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements optimized code to fill a region of memory with a specific byte.
 *
 * PARAMETERS:
 *     (%rcx) dest - Destination buffer.
 *     (%rdx) c - Byte to be used as pattern.
 *     (%r8) count - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     (%rax) Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
.seh_proc memset
.global memset
.align 16
memset:
    .seh_endprologue

    /* Byte pattern for storing last 4-15 bytes. */
    movzx %dl, %edx
    mov $0x0101010101010101, %rax
    imul %rax, %rdx

    /* Byte pattern for SSE instructions. */
    movq %rdx, %xmm0
    movlhps %xmm0, %xmm0

    mov %rcx, %rax
    cmp $64, %r8
    jl .Lsmall16

    /* Our hot loop uses aligned SSE instructions, so let's try aligning the output pointer
     * beforehand. */
    movups %xmm0, (%rcx)
    add %rcx, %r8
    add $16, %rcx
    and $0x0FFFFFFFFFFFFFFF0, %rcx
    sub %rcx, %r8

    /* Break out if alignining the address made our size too small. */
    cmp $64, %r8
    jl .Lsmall16_nocmp

    /* Save the end point of the writes so that we can finish everything without any
     * branches (outside of the main loop). */
    lea -16(%rcx, %r8), %r9
    lea -48(%rcx, %r8), %r10
    and $0x0FFFFFFFFFFFFFFF0, %r10
    shr $6, %r8

.align 16
.Lhot:
    /* Main hot loop, copy 64 bytes per iteration (with aligned instructions). */
    movaps %xmm0, (%rcx) 
    movaps %xmm0, 16(%rcx)
    movaps %xmm0, 32(%rcx)
    movaps %xmm0, 48(%rcx)
    add $64, %rcx
    sub $1, %r8
    jnz .Lhot

.Ltrail64:
    /* We now have between 0-63 trailing bytes. Repeated/overlapping store operations are
     * less expensive than branches/branch mispredictions; We use 4 ops in total here so that
     * we don't need any branch. */
    movaps %xmm0, (%r10)
    movaps %xmm0, 16(%r10)
    movaps %xmm0, 32(%r10)
    movups %xmm0, (%r9)
    ret

.align 2
.Lsmall16:
    cmp $16, %r8
    jl .Lsmall4

.align 2
.Lsmall16_nocmp:
    /* We have between 16-63 bytes, use 4 possibly/probably overlapping writes to handle it. */
    lea -16(%rcx, %r8), %r9
    and $0x20, %r8
    shr $1, %r8

    movups %xmm0, (%rcx)
    movups %xmm0, (%r9)
    movups %xmm0, (%rcx, %r8)
    neg %r8
    movups %xmm0, (%r9, %r8)

    ret

.align 2
.Lsmall4:
    cmp $4, %r8
    jl .Lsmall3

    /* We have between 4-15 bytes, use same technique as above (but with 32-bit values). */
    lea -4(%rcx, %r8), %r9
    and $0x08, %r8
    shr $1, %r8

    mov %edx, (%rcx)
    mov %edx, (%r9)
    mov %edx, (%rcx, %r8)
    neg %r8
    mov %edx, (%r9, %r8)

    ret

.align 2
.Lsmall3:
    /* We have between 0-3 bytes, worst case (for us), write the first byte followed by the
     * last two. */

    test %r8, %r8
    jz .Ledata
    mov %dl, (%rcx)

    cmp $2, %r8
    jl .Ledata
    mov %dx, -2(%rcx, %r8)

.align 2
.Ledata:
    ret
.seh_endproc
