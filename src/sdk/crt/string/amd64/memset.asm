; SPDX-FileCopyrightText: (C) 2023 yuuma03
; SPDX-License-Identifier: BSD-3-Clause

.code

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE: 
;     This function implements optimized code to fill a region of memory with a specific byte.
;
; PARAMETERS:
;     dest (rcx) - Destination buffer.
;     c (rdx) - Byte to be used as pattern.
;     count (r8) - Size of the destination buffer.
;
; RETURN VALUE: 
;     Start of the destination buffer.
;---------------------------------------------------------------------------------------------------------------------
memset proc
    ; Byte pattern for storing last 4-15 bytes.
    movzx edx, dl
    mov rax, 0101010101010101h
    imul rdx, rax

    ; Byte pattern for SSE instructions.
    movq xmm0, rdx
    movlhps xmm0, xmm0

    mov rax, rcx
    cmp r8, 64
    jl small16

    ; Our hot loop uses aligned SSE instructions, so let's try aligning the output pointer beforehand.
    movups [rcx], xmm0
    add r8, rcx
    add rcx, 16
    and rcx, 0FFFFFFFFFFFFFFF0h
    sub r8, rcx

    ; Break out if alignining the address made our size too small.
    cmp r8, 64
    jl small16_nocmp

    ; Save the end point of the writes so that we can finish everything without any branch (outside of the main loop).
    lea r9, [rcx + r8 - 16]
    lea r10, [rcx + r8 - 48]
    and r10, 0FFFFFFFFFFFFFFF0h
    shr r8, 6

hot:
    ; Main hot loop, copy 64 bytes per iteration (with aligned instructions).
    movaps [rcx], xmm0
    movaps [rcx + 16], xmm0
    movaps [rcx + 32], xmm0
    movaps [rcx + 48], xmm0
    add rcx, 64
    sub r8, 1
    jnz hot

trail64:
    ; We now have between 0-63 trailing bytes.
    ; Repeated/overlapping store operations are less expensive than branches/branch mispredictions; We use 4 ops in
    ; total here so that we don't need any branch.
    movaps [r10], xmm0
    movaps [r10 + 16], xmm0
    movaps [r10 + 32], xmm0
    movups [r9], xmm0
    ret

small16:
    cmp r8, 16
    jl small4

small16_nocmp:
    ; We have between 16-63 bytes, use 4 possibly/probably overlapping writes to handle it.
    lea r9, [rcx + r8 - 16]
    and r8, 20h
    shr r8, 1

    movups [rcx], xmm0
    movups [r9], xmm0
    movups [rcx + r8], xmm0
    neg r8
    movups [r9 + r8], xmm0

    ret

small4:
    cmp r8, 4
    jl small3

    ; We have between 4-15 bytes, use same technique as above (but with 32-bit values).
    lea r9, [rcx + r8 - 4]
    and r8, 8h
    shr r8, 1

    mov [rcx], edx
    mov [r9], edx
    mov [rcx + r8], edx
    neg r8
    mov [r9 + r8], edx

    ret

small3:
    ; We have between 0-3 bytes, worst case (for us), write the first byte followed by the last two.

    test r8, r8
    jz edata
    mov [rcx], dl

    cmp r8, 2
    jl edata
    mov [rcx + r8 - 2], dx

edata:
    ret
memset endp

end
