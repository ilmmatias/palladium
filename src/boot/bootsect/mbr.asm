; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: GPL-3.0-or-later

.model tiny
.386p

_TEXT segment use16
org 600h
_TEXT$Start:

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the necessary code to bootstrap the PBR (as if it was loaded by the
;     BIOS).
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     Does not return.
;--------------------------------------------------------------------------------------------------
Main proc
    ; We we're loaded at 7C00h (or some combination of CS+IP that results in that), relocate upper,
    ; as the PBR will use that location.
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov cx, 512
    mov si, 7C00h
    mov di, 600h
    rep movsb
    db 0EAh
    dw Main$Setup, 0

Main$Setup:
    sti
    mov [BootDrive], dl

    ; We only support booting the first active partition at the moment, we error out if we don't
    ; find any.
    mov si, offset FirstPartition
    mov cx, 4
Main$CheckPartition:
    mov al, [si]
    test al, 80h
    jnz Main$FoundPartition

    add si, 10h
    dec cx
    loop Main$CheckPartition

    mov si, offset PartitionError
    jmp Error

Main$FoundPartition:
    mov eax, [si + 8]
    mov bx, 07C00h
    call ReadSector

    mov dl, [BootDrive]
    db 0EAh
    dw 07C00h, 0
Main endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function read a single sector from the boot disk. The input should be an LBA value.
;
; PARAMETERS:
;     Sector (eax) - Which sector to read the data from.
;     Buffer (es:bx) - Destination of the data.
;
; RETURN VALUE:
;     Does not return on failure.
;--------------------------------------------------------------------------------------------------
ReadSector proc
    pushad

    ; We just need to mount the disk address packet and call int 13h.
    db 66h
    push 0
    push eax
    push es
    push bx
    push 1
    push 16

    mov ah, 42h
    mov dl, [BootDrive]
    mov si, sp
    int 13h

    popa
    popad
    jnc ReadSector$End
    mov si, offset DiskError
    jmp Error

ReadSector$End:
    ret
ReadSector endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function should be called when something in the load process goes wrong. It prints the
;     contents of Message and halts the system.
;
; PARAMETERS:
;     Message (si) - Error message to print.
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

BootDrive db 0
DiskError db "Disk error", 0
PartitionError db "No active partitions", 0

.errnz ($ - _TEXT$Start) gt 440
org 7B8h

MbrData label byte
    DiskId dd 0
    Reserved dw 0
    FirstPartition db 16 dup (0)
    SecondPartition db 16 dup (0)
    ThirdPartition db 16 dup (0)
    FourthPartition db 16 dup (0)

dw 0AA55h

_TEXT ends
end
