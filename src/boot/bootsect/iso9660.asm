; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.model tiny
.386p

_TEXT segment use16
org 7C00h

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the necessary code to load the second stage loader and execute it.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     Does not return.
;---------------------------------------------------------------------------------------------------------------------
Main proc
    ; Some BIOSes might load us to 07C0:0000, but our code expects the cs to be 0 (so the BPB would have been loaded
    ; to 0000:7C00), so let's do a far jump just to make sure.
    db 0EAh
    dw Main$Setup, 0

Main$Setup:
    ; Unknown register states are not cool, get all the segment registers ready, and setup a valid stack (ending
    ; behind us).
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 7C00h

    push dx
    mov [BootDrive], dl

    ; The volume descriptors start at sector 0x10, we want to find the one with type==1, as it contains the root
    ; directory LBA.
    mov eax, 10h

Main$SearchRoot:
    mov bx, 600h
    mov si, bx
    mov cx, 1
    call ReadSectors

    mov bl, [si]
    cmp bl, 1
    je Main$FoundRoot

    ; Type 0xFF is bad (for us), means that somehow the image doesn't have a root directory.
    cmp bl, 0FFh
    jne Main$SearchRoot
    mov si, offset ImageError
    jmp Error

Main$FoundRoot:
    ; Let's grab the root directory location, and search for our image/file to load.
    mov eax, [si + 158]

Main$NextSector:
    mov bx, 600h
    mov si, bx
    mov cx, 1
    call ReadSectors

Main$Search:
    xor ax, ax
    mov al, [si]
    cmp al, 0
    je Main$NotFound

    test byte ptr [si + 25], 2
    jnz Main$Next

    cmp byte ptr [si + 32], ImageNameSize
    jne Main$Next

    push si
    add si, 33
    mov di, offset ImageName
    mov cx, ImageNameSize
    repe cmpsb
    pop si
    jne Main$Next

    ; Finish up by loading the file (right after the boot sector) and transfering execution.
    ; Just take a lot of caution with the size, it could be 32-bits in theory, but we're treating the image as
    ; invalid if that happens (why would our loader be >64KiB anyways?).
    ; It also might overflow when rounding up, handle that manually too.
    mov cx, [si + 12]
    cmp cx, 0
    jne Main$Overflow

    mov cx, [si + 10]
    cmp cx, 0F801h
    jae Main$BigButValid

    add cx, 2047
    shr cx, 11
    jmp Main$Load

Main$BigButValid:
    mov cx, 32

Main$Load:
    mov eax, [si + 2]
    mov bx, 4000h
    mov es, bx
    xor bx, bx
    call ReadSectors

    pop dx
    push 4000h
    push 0
    retf

Main$Next:
    add si, ax
    cmp si, bx
    jb Main$Search
    jmp Main$NextSector

Main$Overflow:
    mov si, offset OverflowError
    jmp Error

Main$NotFound:
    mov si, offset ImageError
    jmp Error
Main endp

ReadSectors proc
;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function reads sectors from the boot disk. The input should be an LBA value.
;
; PARAMETERS:
;     FirstSector (eax) - Which sector to start reading from.
;     Buffer (es:bx) - Destination of the read data.
;     SectorCount (cx) - How many sectors to read.
;
; RETURN VALUE:
;     Does not return on failure, all of the inputs are already incremented on success.
;---------------------------------------------------------------------------------------------------------------------

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
    jnc ReadSectors$Next
    mov si, offset DiskError
    jmp Error

ReadSectors$Next:
    inc eax
    add bx, 2048
    jnc ReadSectors$NoOverflow

    ; We overflowed in the low 16-bits of the address.
    ; Let's try to bump up the high 4 bits (segment), if we also overflowed there, then we crash (data too big for
    ; us).

    push bx
    mov bx, es
    add bx, 1000h
    cmp bx, 0A000h
    jbe ReadSectors$NoOverflowSegment
    mov si, offset OverflowError
    jmp Error

ReadSectors$NoOverflowSegment:
    mov es, bx
    pop bx

ReadSectors$NoOverflow:
    loop ReadSectors
    ret
ReadSectors endp

Error proc
;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function should be called when something in the load process goes wrong. It prints the contents of
;     Message and halts the system.
;
; PARAMETERS:
;     Message (si) - Error message to print.
;
; RETURN VALUE:
;     Does not return.
;---------------------------------------------------------------------------------------------------------------------

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
OverflowError db "Data overflow", 0
ImageError db "BOOTMGR is missing", 0
ImageName db "BOOTMGR.;1"
ImageNameSize equ $ - ImageName

.errnz ($ - Main) gt 510
org 7DEEh
dw 0AA55h

_TEXT ends
end
