; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function inserts a new E820 style memory region to the list (while mantaining the list sorted).
;
; PARAMETERS:
;     Entry (es:di) - entry to add; di used as it's where E820h returns its value (+ it frees us from using
;                     registers).
;
; RETURN VALUE:
;     None.
;---------------------------------------------------------------------------------------------------------------------
InsertRegion proc
    push si
    push di
    push eax
    push ecx
    push edx

    mov si, 4014h
    mov ecx, dword ptr [BootBlock$MemoryCount]
    or ecx, ecx
    jz InsertRegion$New

InsertRegion$Loop:
    ; We shouldn't have this situation, but check for contiguous entries (our base == their base+length).
    mov eax, dword ptr ds:[si + 16]
    cmp eax, dword ptr es:[di + 16]
    jne InsertRegion$CheckAbove

    mov eax, dword ptr ds:[si]
    mov edx, dword ptr ds:[si + 4]
    add eax, dword ptr ds:[si + 8]
    adc edx, dword ptr ds:[si + 12]

    cmp eax, dword ptr es:[di]
    jne InsertRegion$CheckUs
    cmp edx, dword ptr es:[di + 4]
    jne InsertRegion$CheckUs

InsertRegion$AppendSize:
    ; Buggy/badly programmed BIOS I guess?
    mov eax, dword ptr es:[di + 8]
    mov edx, dword ptr es:[di + 12]
    add dword ptr ds:[si + 8], eax
    adc dword ptr ds:[si + 12], edx
    jmp InsertRegion$End

InsertRegion$CheckUs:
    ; Save as above, but their base == our base+length
    mov eax, dword ptr es:[di]
    mov edx, dword ptr es:[di + 4]
    add eax, dword ptr es:[di + 8]
    adc edx, dword ptr es:[di + 12]

    cmp eax, dword ptr ds:[si]
    jne InsertRegion$CheckAbove
    cmp edx, dword ptr ds:[si + 4]
    jne InsertRegion$CheckAbove

    mov eax, dword ptr es:[di]
    mov dword ptr ds:[si], eax
    mov eax, dword ptr es:[si + 4]
    mov dword ptr ds:[si + 4], eax
    jmp InsertRegion$AppendSize

InsertRegion$CheckAbove:
    ; Higher half of the address can already say if we're bigger (or smaller).
    ; Only if they are equal we need to check the lower half.
    mov eax, dword ptr ds:[si + 4]
    cmp eax, dword ptr es:[di + 4]
    ja InsertRegion$Before
    jb InsertRegion$Next

    mov eax, dword ptr ds:[si]
    cmp eax, dword ptr es:[di]
    jae InsertRegion$Before

InsertRegion$Next:
    add si, 20
    dec ecx
    jnz InsertRegion$Loop
    jmp InsertRegion$New

InsertRegion$Before:
    ; We should only reach this place if we need to relocate all entries (move them forward).
    ; movsb can be done backwards if the direction flag is set (which we will do).

    push si
    push di
    std

    ; Hope it fits I guess?
    mov ax, 20
    mul cx
    mov cx, ax
    add si, ax
    mov di, si
    add di, 20
    rep movsb

    cld
    pop di
    pop si

InsertRegion$New:
    xchg si, di
    mov cx, 20
    rep movsb
    inc dword ptr [BootBlock$MemoryCount]

InsertRegion$End:
    pop edx
    pop ecx
    pop eax
    pop di
    pop si
    ret
InsertRegion endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to find a memory region containing the given address range.
;
; PARAMETERS:
;     Base (ebx) - base address of the range.
;     Length (edx) - length of the range.
;
; RETURN VALUE:
;     None.
;---------------------------------------------------------------------------------------------------------------------
FindRegion proc
    push di
    push eax
    push ecx
    push ebp

    mov di, 4014h
    mov ecx, dword ptr [BootBlock$MemoryCount]
    or ecx, ecx
    jz FindRegion$Fail

FindRegion$Loop:
    ; base > 32bits means fail (as no way it+the length will somehow contain the wanted range.
    cmp dword ptr es:[di + 4], 0
    jne FindRegion$Fail

    ; length>32bits we can probably assume we're inside the upper range.
    cmp dword ptr es:[di + 12], 0
    jne FindRegion$CheckLower

    ; length<32bits still needs to be done with caution, as it might overflow.
    mov eax, dword ptr es:[di]
    add eax, dword ptr es:[di + 8]
    jo FindRegion$CheckLower

    mov ebp, ebx
    add ebp, edx
    ; Overflow is not really supposed to happen, but let's take that as an error/region is too big to possibly fit.
    jo FindRegion$Fail
    cmp ebp, eax
    jge FindRegion$CheckLower

    loop FindRegion$Loop

FindRegion$CheckLower:
    mov eax, dword ptr es:[di]
    cmp eax, ebx
    jbe FindRegion$Found

FindRegion$Fail:
    pop ebp
    pop ecx
    pop eax
    pop di
    stc
    ret

FindRegion$Found:
    pop ebp
    pop ecx
    pop eax
    pop di
    clc
    ret
FindRegion endp
