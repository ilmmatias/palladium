; SPDX-FileCopyrightText: (C) 2023 yuuma03
; SPDX-License-Identifier: BSD-3-Clause

.code

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements optimized code to copy bytes from one region of memory to another.
;
; PARAMETERS:
;     dest (rcx) - Destination buffer.
;     src (rdx) - Source buffer.
;     count (r8) - How many bytes to copy.
;
; RETURN VALUE:
;     Start of the destination buffer.
;---------------------------------------------------------------------------------------------------------------------
memcpy proc
    mov rax, rcx
    cmp r8, 64
    jl small16

    ; Our hot loop uses aligned SSE instructions, so let's try aligning the output pointer beforehand.
    movups xmm0, [rdx]
    movups [rcx], xmm0
    add rcx, 16
    and rcx, 0FFFFFFFFFFFFFFF0h
    add rdx, rcx
    sub rdx, rax
    sub r8, rcx
    add r8, rax

    ; Break out if alignining the address made our size too small.
    cmp r8, 64
    jl small16_nocmp

    ; Save the end point of the writes so that we can finish everything without any branch (outside of the main loop).
    push r12
    lea r9, [rcx + r8 - 16]
    lea r10, [rcx + r8 - 48]
    and r10, 0FFFFFFFFFFFFFFF0h
    lea r11, [rdx + r8 - 16]
    sub r10, rcx
    lea r12, [rdx + r10]
    add r10, rcx
    shr r8, 6

hot:
    ; Main hot loop, copy 64 bytes per iteration (with aligned instructions on the dest).
    movups xmm0, [rdx]
    movups xmm1, [rdx + 16]
    movups xmm2, [rdx + 32]
    movups xmm3, [rdx + 48]
    movaps [rcx], xmm0
    movaps [rcx + 16], xmm1
    movaps [rcx + 32], xmm2
    movaps [rcx + 48], xmm3
    add rcx, 64
    add rdx, 64
    sub r8, 1
    jnz hot

trail64:
    ; We now have between 0-63 trailing bytes.
    ; Repeated/overlapping store operations are less expensive than branches/branch mispredictions; We use
    ; 4 read ops+4 write ops in total here so that we don't need any branch.
    movups xmm0, [r12]
    movups xmm1, [r12 + 16]
    movups xmm2, [r12 + 32]
    movups xmm3, [r11]
    movaps [r10], xmm0
    movaps [r10 + 16], xmm1
    movaps [r10 + 32], xmm2
    movups [r9], xmm3
    pop r12
    ret

small16:
    cmp r8, 16
    jl small4

small16_nocmp:
    ; We have between 16-63 bytes, use 4 possibly/probably overlapping writes to handle it.
    lea r9, [rcx + r8 - 16]
    lea r10, [rdx + r8 - 16]
    and r8, 20h
    shr r8, 1

    movups xmm0, [rdx]
    movups xmm1, [r10]
    movups xmm2, [rdx + r8]
    movups [rcx], xmm0
    movups [r9], xmm1
    movups [rcx + r8], xmm2
    neg r8
    movups xmm3, [r10 + r8]
    movups [r9 + r8], xmm3

    ret

small4:
    cmp r8, 4
    jl small3

    ; We have between 4-15 bytes, use same technique as above (but with 32-bit values).
    lea r9, [rcx + r8 - 4]
    lea r10, [rdx + r8 - 4]
    and r8, 8h
    shr r8, 1

    mov r11d, [rdx]
    mov [rcx], r11d
    mov r11d, [r10]
    mov [r9], r11d
    mov r11d, [rdx + r8]
    mov [rcx + r8], r11d
    neg r8
    mov r11d, [r10 + r8]
    mov [r9 + r8], r11d

    ret

small3:
    ; We have between 0-3 bytes, worst case (for us), write the first byte followed by the last two.

    test r8, r8
    jz edata
    mov r9b, [rdx]
    mov [rcx], r9b

    cmp r8, 2
    jl edata
    mov r9w, [rdx + r8 - 2]
    mov [rcx + r8 - 2], r9w

edata:
    ret
memcpy endp

end
