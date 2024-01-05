; SPDX-FileCopyrightText: (C) 2022-2023 ilmmatias
; SPDX-License-Identifier: GPL-3.0-or-later

.model tiny
.x64p

_TEXT16 segment use16
org 0
_TEXT16$Start:

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the necessary code to prepare the processor and environment to
;     reload and jump into bootmgr.exe.
;
; PARAMETERS:
;     BootDrive (dl) - Boot drive BIOS index.
;
; RETURN VALUE:
;     This function does not return.
;--------------------------------------------------------------------------------------------------
Main proc
    mov ax, cs
    mov ds, ax
    xor ax, ax
    mov es, ax
    mov byte ptr [BootDrive], dl

    ; We prefer no text cursor (as we want to show the menu with no text cursor on bootmgr).
    mov ah, 1
    mov ch, 3Fh
    int 10h

    call CheckA20
    jnc Main$A20Enabled
    call TryA20Bios
    jnc Main$A20Enabled
    call TryA20Keyboard
    jnc Main$A20Enabled

    mov si, offset A20Error
    call Error

Main$A20Enabled:
    ; startup.com has its size fixed at 2KiB, the PE file (bootmgr.exe) should have been copied
    ; right after that.
    mov esi, _TEXT16$End + (_TEXT32$PeStart - _TEXT32$Start)
    add esi, dword ptr [esi + 3Ch]

    ; Our code segment isn't 0, so we need to fix up the GDT, all of our other data structures, and
    ; the far jump IP.
    cli

    mov eax, cs
    shl eax, 4
    add esi, eax

    movzx ebx, byte ptr [BootDrive]
    push ebx

    mov ebx, eax
    add ebx, offset _TEXT16$End
    add [GdtBase], eax
    lgdt fword ptr [GdtSize]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    pushd 8
    push ebx
    mov bp, sp
    jmp far32 ptr [bp]
Main endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function checks if the A20 line (which allows access to addresses over the 1MiB
;     barrier) is enabled.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if the A20 line is disabled.
;--------------------------------------------------------------------------------------------------
CheckA20 proc
    ; We do the check by comparing the memory at the end of the bootsector (0xAA55) with the
    ; memory at the same address but 1MiB higher.
    ; Just to make sure we don't have some kind of one off coincidence, we modify said value
    ; after checking it once, and then check it again.

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

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to enable the A20 line using the BIOS int 15h functions.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't enable the A20 line.
;--------------------------------------------------------------------------------------------------
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

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function tries to enable the A20 line using the keyboard controller (yes, the KB
;     controller).
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     CF set if we couldn't enable the A20 line.
;--------------------------------------------------------------------------------------------------
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

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function should be called when something in the load process goes wrong. It prints the
;     contents of Message and halts the system.
;
; PARAMETERS:
;     Message (si) - error message to print.
;
; RETURN VALUE:
;     Does not return.
;--------------------------------------------------------------------------------------------------
Error proc
    lodsb
    or al, al
    jz Error$Halt
    mov ah, 0Eh
    xor bh, bh
    int 10h
    jmp Error

Error$Halt:
    jmp $
Error endp

A20Error db "Couldn't enable the processor A20 line.", 0

align 8
GdtDescs dq 0000000000000000h, 00CF9A000000FFFFh, 00CF92000000FFFFh, 008F9A000000FFFFh, 008F92000000FFFFh
GdtSize dw GdtSize - GdtDescs - 1
GdtBase dd offset GdtDescs

BootDrive db 0

_TEXT16$End:
_TEXT16 ends

_TEXT32 segment byte use32
_TEXT32$Start:

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the necessary code to relocate bootmgr.exe into himem and transfer
;     execution to it.
;
; PARAMETERS:
;     None
;
; RETURN VALUE:
;     This function does not return.
;--------------------------------------------------------------------------------------------------
Relocate proc
    ; We just loaded the new GDT, so we need to fix our segment registers.
    mov ax, 10h
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    add esp, 8
    pop eax
    mov esp, 7C00h
    sub esp, 8
    push eax

    mov eax, dword ptr [esi + 40]
    add eax, dword ptr [esi + 52]
    call eax
    jmp $
Relocate endp

_TEXT32$End:
byte 2048 - (_TEXT16$End - _TEXT16$Start) - (_TEXT32$End - _TEXT32$Start) dup (90h)
_TEXT32$PeStart:

_TEXT32 ends

end
