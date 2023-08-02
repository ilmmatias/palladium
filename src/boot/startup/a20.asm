; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function checks if the A20 line (which allows access to addresses over the 1MiB barrier) is enabled.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if the A20 line is disabled.
;---------------------------------------------------------------------------------------------------------------------
CheckA20 proc
    ; We do the check by comparing the memory at the end of the bootsector (0xAA55) with the memory at the same
    ; address but 1MiB higher.
    ; Just to make sure we don't have some kind of one off coincidence, we modify said value after checking it
    ; once, and then check it again.

    push ax
    push si
    push di

    mov ax, es
    push es
    mov ax, ds
    push ds

    mov ax, 0FFFFh
    mov es, ax
    xor ax, ax
    mov ds, ax
    mov si, 7DFEh
    mov di, 7E0Eh

    mov ax, [si]
    cmp ax, es:[di]
    jne CheckA20$Enabled

    mov [si], si
    cmp si, es:[di]
    jne CheckA20$Enabled

    pop ax
    mov ds, ax
    pop ax
    mov es, ax

    pop di
    pop si
    pop ax

    stc
    ret

CheckA20$Enabled:
    pop ax
    mov ds, ax
    pop ax
    mov es, ax

    pop di
    pop si
    pop ax

    clc
    ret
CheckA20 endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to enable the A20 line using the BIOS int 15h functions.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't enable the A20 line.
;---------------------------------------------------------------------------------------------------------------------
TryA20Bios proc
    push ax
    push bx

    ; QUERY A20 GATE SUPPORT
    mov ax, 2403h
    int 15h
    jc TryA20Bios$Fail
    or ah, ah
    jnz TryA20Bios$Fail

    ; GET A20 GATE STATUS
    mov ax, 2402h
    int 15h
    jc TryA20Bios$Fail
    or ah, ah
    jnz TryA20Bios$Fail
    or al, al
    jnz TryA20Bios$Enabled

    ; ENABLE A20 GATE
    mov ax, 2401h
    int 15h
    jc TryA20Bios$Fail
    or ah, ah
    jnz TryA20Bios$Fail

TryA20Bios$Enabled:
    pop bx
    pop ax

    ; We don't trust the BIOS, call CheckA20 just to be sure.
    call CheckA20
    ret

TryA20Bios$Fail:
    pop bx
    pop ax

    stc
    ret
TryA20Bios endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to enable the A20 line using the keyboard controller (yes, the KB controller).
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't enable the A20 line.
;---------------------------------------------------------------------------------------------------------------------
TryA20Keyboard proc
    push ax
    push cx
    cli

    call TryA20Keyboard$Wait
    mov al, 0ADh
    out 64h, al

    call TryA20Keyboard$Wait
    mov al, 0D0h
    out 64h, al

    call TryA20Keyboard$Wait2
    in al, 60h
    push ax

    call TryA20Keyboard$Wait
    mov al, 0D1h
    out 64h, al

    call TryA20Keyboard$Wait
    pop ax
    or al, 2
    out 60h, al

    call TryA20Keyboard$Wait
    mov al, 0AEh
    out 64h, al

    call TryA20Keyboard$Wait
    sti

    ; We're done, but the KB might be slow, so check on a timeout (5 times).
    mov cx, 5

TryA20Keyboard$Loop:
    call CheckA20
    jnc TryA20Keyboard$Finish
    loop TryA20Keyboard$Loop

TryA20Keyboard$Finish:
    pop cx
    pop ax
    ret

TryA20Keyboard$Wait:
    in al, 64h
    test al, 2
    jnz TryA20Keyboard$Wait
    ret

TryA20Keyboard$Wait2:
    in al, 64h
    test al, 1
    jz TryA20Keyboard$Wait2
    ret
TryA20Keyboard endp
