; SPDX-FileCopyrightText: (C) 2022-2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.model tiny
.x64p

_TEXT16 segment use16
org 0

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
    mov byte ptr [BootBlock$BootDrive], dl

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
    ; We try using E820h to get low+high mem map, if that fails, use int 12h + one of the other 2
    ; methods to get the high mem.

    call TryE820
    jnc Main$GotMemMap
    call GetLowMap
    jc Main$NoMemMap
    call TryE801
    jnc Main$GotMemMap
    call Try88
    jnc Main$GotMemMap

Main$NoMemMap:
    mov si, offset MapError
    call Error

Main$GotMemMap:
    mov dword ptr [BootBlock$MemoryRegions], 4014h

    ; Checking for validity of the PE file (and its sections) is our job, as the code at
    ; _TEXTJ onwards assumes everything is valid.
    mov esi, _TEXT16$End + (_TEXTJ$End - _TEXTJ$Start)
    cmp word ptr [esi], 5A4Dh
    je Main$MzFound
    mov si, offset MzError
    call Error

Main$MzFound:
    push esi
    add esi, dword ptr [esi + 3Ch]
    cmp dword ptr [esi], 4550h
    jne Main$BadSig

    cmp word ptr [esi + 24], 10Bh
    je Main$GoodSig

Main$BadSig:
    mov si, offset MagicError
    call Error

Main$GoodSig:
    cmp word ptr [esi + 4], 14Ch
    je Main$MachMatch
    mov si, offset MachineError
    call Error

Main$MachMatch:
    cmp word ptr [esi + 92], 1
    je Main$SubsysMatch
    mov si, offset SubsystemError
    call Error

Main$SubsysMatch:
    ; Every single section should be inside a free memory region.
    ; The memory regions should be contigous (and the biggest size possible), so we can just
    ; find the first region that could fit the section, and check if we really fit inside it.
    movzx ecx, word ptr [esi + 6]
    movzx eax, word ptr [esi + 20]
    add eax, esi
    add eax, 24

    ; I don't think this should happen?
    or ecx, ecx
    jnz Main$Loop
    mov si, offset NoSectionError
    call Error

Main$Loop:
    mov ebx, dword ptr [eax + 12]
    add ebx, dword ptr [esi + 52]
    mov edx, dword ptr [eax + 8]
    call FindRegion
    jnc Main$Next

    mov si, offset SectionError
    call Error

Main$Next:
    loop Main$Loop

Main$Continue:
    ; This whole code is loaded at >64K (using CS != 0), we need to fix up the GDT, all of
    ; our other data structures, and the far jump IP.
    cli

    mov eax, cs
    shl eax, 4
    add esi, eax

    pop ebp
    mov ebx, offset BootBlock
    add ebx, eax
    add ebp, eax
    push ebx
    push ebp

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

include a20.asm
include memmap.asm
include region.asm

A20Error db "Couldn't enable the processor A20 line.", 0
MapError db "Couldn't get the system memory map.", 0
MzError db "The MZ signature in bootmgr.exe is invalid.", 0
MagicError db "The PE signature in bootmgr.exe is invalid.", 0
MachineError db "The target machine in bootmgr.exe is wrong.", 0
SubsystemError db "The target subsystem in bootmgr.exe is wrong.", 0
NoSectionError db "There are no sections in bootmgr.exe.", 0
SectionError db "One of the bootmgr.exe sections doesn't fit inside the physical memory.", 0

align 8
GdtDescs dq 0000000000000000h, 00CF9A000000FFFFh, 00CF92000000FFFFh, 008F9A000000FFFFh, 008F92000000FFFFh
GdtSize dw GdtSize - GdtDescs - 1
GdtBase dd offset GdtDescs

BootBlock:
    BootBlock$BootDrive db 0
    BootBlock$MemoryCount dd 0
    BootBlock$MemoryRegions dd 0

_TEXT16$End:
_TEXT16 ends

_TEXTJ segment byte use32
_TEXTJ$Start:

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
    pop ebp
    mov ebx, esi
    movzx ecx, word ptr [ebx + 6]
    movzx eax, word ptr [ebx + 20]
    add esi, eax
    add esi, 24

Relocate$Load:
    ; Diff between virtual and physical size means that we should zero-clean part of the section.
    ; We handle that by always just cleaning the whole section before loading it.

    push ecx
    push esi

    xor eax, eax
    mov ecx, dword ptr [esi + 8]
    mov edi, dword ptr [esi + 12]
    add edi, dword ptr [ebx + 52]
    rep stosb

    mov ecx, dword ptr [esi + 16]
    mov edi, dword ptr [esi + 12]
    add edi, dword ptr [ebx + 52]
    mov eax, ebp
    add eax, dword ptr [esi + 20]
    mov esi, eax
    rep movsb

    pop esi
    pop ecx
    add esi, 40
    loop Relocate$Load

Relocate$Transfer:
    pop eax
    and esp, -16
    sub esp, 4
    push eax

    mov eax, dword ptr [ebx + 40]
    add eax, dword ptr [ebx + 52]
    jmp eax
    jmp $
Relocate endp

_TEXTJ$End:
_TEXTJ ends

end
