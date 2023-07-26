; SPDX-FileCopyrightText: (C) 2023 yuuma03
; SPDX-License-Identifier: BSD-3-Clause

.code

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements optimized code to determine the size of a C string.
;
; PARAMETERS:
;     str (rcx) - Source string.
;
; RETURN VALUE:
;     Size in bytes of the given string (excluding the \0 at the end).
;---------------------------------------------------------------------------------------------------------------------
strlen proc
    ; pcmpeqb using a memory address (instead of XMM registers) uses aligned memory.
    ; We could use movups, but as it reads 16 bytes at time, there's the chance the byte right before crossing a page
    ; is zero (which is what we want to find), and the next byte (which resides in the next page) is unmapped.
    ; As such, aligning the pointer down is useful both for speed on the main loop, and for preventing such page
    ; faults.
    xor rax, rax
    mov rdx, rcx
    and rdx, 0FFFFFFFFFFFFFFF0h
    sub rcx, rdx

    ; Unrolled initial iteration, as we need to discard part of the value (we aligned down, so the current block might
    ; have some bytes that are not part of the string.
    pxor xmm0, xmm0
    pxor xmm1, xmm1
    pxor xmm2, xmm2
    pxor xmm3, xmm3
    pcmpeqb xmm0, [rdx]
    pmovmskb r8, xmm0
    shr r8, cl

    test r8, r8
    jz prolog
    bsf ax, r8w
    ret

prolog:
    pxor xmm0, xmm0
    mov rax, 16
    sub rax, rcx
    add rdx, 16

    ; Main loop unrolled * 4 below, slower than the std implementation from my host OS libc, but for now, good enough
    ; for us. Later on we could try optimizing it to read 64-bytes at a time (and align the pointer to 64-bytes).
    ; I tried a dummy approach (really only changing that), and it was slower, so there's still some more work needed.

cmp_0_to_15:
    pcmpeqb xmm0, [rdx]
    pmovmskb r8, xmm0
    test r8, r8
    jz cmp_16_to_31
    bsf r8w, r8w
    add rax, r8
    ret

cmp_16_to_31:
    pcmpeqb xmm1, [rdx + 16]
    pmovmskb r8, xmm1
    test r8, r8
    jz cmp_32_to_47
    bsf r8w, r8w
    add rax, r8
    add rax, 16
    ret

cmp_32_to_47:
    pcmpeqb xmm2, [rdx + 32]
    pmovmskb r8, xmm2
    test r8, r8
    jz cmp_48_to_63
    bsf r8w, r8w
    add rax, r8
    add rax, 32
    ret

cmp_48_to_63:
    pcmpeqb xmm3, [rdx + 48]
    pmovmskb r8, xmm3
    test r8, r8
    jz next
    bsf r8w, r8w
    add rax, r8
    add rax, 48
    ret

next:
    add rdx, 64
    add rax, 64
    jmp cmp_0_to_15
strlen endp

end
