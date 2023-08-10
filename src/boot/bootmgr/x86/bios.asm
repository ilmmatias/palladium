; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.code

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function drops the processor back to real mode, and executes a BIOS interrupt with the
;     given processor context.
;
; PARAMETERS:
;     Number (esp + 4) - Interrupt number.
;     Registers (esp + 8) - I/O; State of the processor when executing the interrupt, and state
;                           after.
;
; RETURN VALUE:
;     None.
;--------------------------------------------------------------------------------------------------
_BiosCall proc
    ; We should be in low memory, so start by overwriting the int call number up ahead and saving
    ; the regs pointer.
    mov al, [esp + 4]
    mov [_BiosCall$Int + 1], al
    mov eax, [esp + 8]
    mov [_BiosCall$InRegs], eax
    add eax, 36
    mov [_BiosCall$OutRegs], eax

    ; Worst case the BIOS might overwrite our GDT, so we need to save that.
    sgdt [_BiosCall$SavedGdt]

    ; We'll popa on real mode, trashing all registers. Save anything the compiler expects us to
    ; not mess with.
    push ebx
    push esi
    push edi
    push ebp

    ; startup.com should have setup our GDT properly, so drop back to protected 16-bits mode.
    mov ax, 08h
    push ax
    lea eax, _BiosCall$End
    push ax
    lea eax, _BiosCall$SavedGdt
    push ax
    xor eax, eax
    push ax
    lea eax, _BiosCall$Real
    push ax
    lea eax, _BiosCall$Protected16
    push 18h
    push eax
    retf

_BiosCall$Protected16:
    ; We also need to load up the data segments.
    db 0B8h, 020h, 0 ; mov ax, 20h
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax

    ; IDT should still be in place, disable the PE bit and drop to real mode.
    mov eax, cr0
    and al, 0FEh
    mov cr0, eax

    retf

_BiosCall$Real:
    ; Zero out all the segments, and we should be ready to reenable interrupts and execute
    ; what was requested.
    xor eax, eax
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax

    db 67h, 89h, 25h ; mov [_BiosCall$SavedEsp], sp
    dd _BiosCall$SavedEsp
    db 67h, 8Bh, 25h ; mov sp, [_BiosCall$InRegs]
    dd _BiosCall$InRegs

    popaw
    popfw

    db 67h, 8Bh, 25h ; mov sp, [_BiosCall$SavedEsp]
    dd _BiosCall$SavedEsp

    sti
_BiosCall$Int:
    int 0
    cli

    db 67h, 8Bh, 25h ; mov sp, [_BiosCall$OutRegs]
    dd _BiosCall$OutRegs

    pushfw
    pushaw

    db 67h, 8Bh, 25h ; mov sp, [_BiosCall$SavedEsp]
    dd _BiosCall$SavedEsp

    ; Restore the GDT and get right back onto pmode (no need to go to 16-bits pmode first).
    pop ebx
    db 0Fh, 01h, 17h ; lgdt [bx]

    mov eax, cr0
    or al, 1
    mov cr0, eax

    retf

_BiosCall$End:
    ; Restore the segments and registers, and give control back to the caller.
    mov ax, 10h
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    pop ebp
    pop edi
    pop esi
    pop ebx

    ret

_BiosCall$SavedGdt dq 0
_BiosCall$SavedEsp dd 0
_BiosCall$InRegs dd 0
_BiosCall$OutRegs dd 0
_BiosCall endp

end
