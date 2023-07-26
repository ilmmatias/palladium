; SPDX-FileCopyrightText: (C) 2023 yuuma03
; SPDX-License-Identifier: BSD-3-Clause

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function gets (and assumes a bit) how much contigous memory we have in the low memory.
;     We need this when not using E820.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't get the memory map.
;---------------------------------------------------------------------------------------------------------------------
GetLowMap proc
    push di
    push eax
    push edx
    mov di, 4000h

    clc
    int 12h
    jc GetLowMap$Fail

    shl eax, 10
    mov dword ptr es:[di], 0
    mov dword ptr es:[di + 4], 0
    mov dword ptr es:[di + 8], eax
    mov dword ptr es:[di + 12], 0
    mov dword ptr es:[di + 16], 1
    call InsertRegion

    mov edx, 100000h
    sub edx, eax
    ; From the end of the contig memory up to 000FFFFF we have the EBDA, reserved hardware mappings, BIOS stuff, etc.
    mov dword ptr es:[di], eax
    mov dword ptr es:[di + 4], 0
    mov dword ptr es:[di + 8], edx
    mov dword ptr es:[di + 12], 0
    mov dword ptr es:[di + 16], 2
    call InsertRegion

    pop edx
    pop eax
    pop di
    clc
    ret

GetLowMap$Fail:
    ; I don't think we need this stc?
    pop edx
    pop eax
    pop di
    stc
    ret
GetLowMap endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to get the full memory map using the E820h BIOS function.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't get the memory map.
;---------------------------------------------------------------------------------------------------------------------
TryE820 proc
    push eax
    push ebx
    push ecx
    push edx
    push di

    ; ebx = 0 means start from the top, other values are all magic to not break anything (though ecx might be
    ; ignored).
    mov eax, 0E820h
    xor ebx, ebx
    mov edx, 534D4150h
    mov ecx, 24
    mov di, 4000h

    ; First call shouldn't set carry (that means E820 is unsupported), it shouldn't return invalid EAX, nor should it
    ; be the last (that also means unsupported/broken implementation, at least for us).
    int 15h
    jc TryE820$Fail
    cmp eax, 0534D4150h
    jne TryE820$Fail
    or ebx, ebx
    jz TryE820$Fail
    jmp TryE820$Loop

TryE820$Next:
    or ebx, ebx
    jz TryE820$Done

    mov eax, 0E820h
    mov ecx, 24
    int 15h
    jc TryE820$Done

TryE820$Loop:
    jcxz TryE820$Next

    ; ecx = 24 means that this is an extended ACPI 3 entry, which can have the "this entry is valid!" bit unset.
    cmp ecx, 24
    jb TryE820$NotAcpi3
    test byte ptr es:[di + 20], 1
    jne TryE820$Next

TryE820$NotAcpi3:
    mov ecx, es:[di + 8]
    or ecx, es:[di + 12]
    jz TryE820$Next

    call InsertRegion
    jmp TryE820$Next

TryE820$Done:
    pop di
    pop edx
    pop ecx
    pop ebx
    pop eax
    clc
    ret

TryE820$Fail:
    pop di
    pop edx
    pop ecx
    pop ebx
    pop eax
    stc
    ret
TryE820 endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to get how much contiguous high memory we have using the E801h BIOS function.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't get the memory map.
;---------------------------------------------------------------------------------------------------------------------
TryE801 proc
    push eax
    push bx
    push cx
    push dx
    push di
    mov di, 4000h

    mov ax, 0E801h
    xor bx, bx
    xor cx, cx
    xor dx, dx
    int 15h
    jc TryE801$Fail

    ; Workaround buggy BIOSes that return in CX and DX instead of AX and BX.
    or ax, ax
    jnz TryE801$Continue

    mov ax, cx
    mov bx, dx

TryE801$Continue:
    movzx eax, ax
    shl eax, 10

    mov dword ptr es:[di], 100000h
    mov dword ptr es:[di + 4], 0
    mov dword ptr es:[di + 8], eax
    mov dword ptr es:[di + 12], 0
    mov dword ptr es:[di + 16], 1
    call InsertRegion

    or bx, bx
    jz TryE801$End

    movzx eax, bx
    shl eax, 16

    mov dword ptr es:[di], 1000000h
    mov dword ptr es:[di + 4], 0
    mov dword ptr es:[di + 8], eax
    mov dword ptr es:[di + 12], 0
    mov dword ptr es:[di + 16], 1
    call InsertRegion

TryE801$End:
    pop di
    pop dx
    pop cx
    pop bx
    pop eax
    clc
    ret

TryE801$Fail:
    pop di
    pop dx
    pop cx
    pop bx
    pop eax
    stc
    ret
TryE801 endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to get how much contiguous high memory we have using the 88h BIOS function.
;     This is the last BIOS function we will try, so it needs to succed (or we will panic/crash).
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't get the memory map.
;---------------------------------------------------------------------------------------------------------------------
Try88 proc
    push ax
    push di
    mov di, 4000h

    clc
    mov ah, 88h
    jc Try88$Fail
    or ax, ax
    jz Try88$Fail

    movzx eax, ax
    shl eax, 10

    mov dword ptr es:[di], 100000h
    mov dword ptr es:[di + 4], 0
    mov dword ptr es:[di + 8], eax
    mov dword ptr es:[di + 12], 0
    mov dword ptr es:[di + 16], 1
    call InsertRegion

    pop di
    pop ax
    clc
    ret

Try88$Fail:
    pop di
    pop ax
    stc
    ret
Try88 endp
