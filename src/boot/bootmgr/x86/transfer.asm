; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.code

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function puts the processor in long mode, and transfers execution away to the kernel.
;
; PARAMETERS:
;     Pml4 (esp + 4) - Physical address of the PML4 structure.
;     EntryPoint (esp + 8) - Virtual address (64-bits) where the kernel entry is located.
;
; RETURN VALUE:
;     Does not return.
;--------------------------------------------------------------------------------------------------
_BiTransferExecution proc
    ; Enable long mode in the EFER.
    mov ecx, 0C0000080h
    rdmsr
    or eax, 100h
    wrmsr

    ; Enable PAE (physical address extension); Required for long mode.
    mov eax, cr4
    or eax, 20h
    mov cr4, eax

    ; Enable the paging flag; CR3 will be set as well, but won't take effect until we jump into
    ; a LM descriptor.
    mov eax, [esp + 4]
    mov cr3, eax
    mov eax, cr0
    or eax, 80000000h
    mov cr0, eax

    ; The GDT last 2 entries should be 64-bits descriptors, so just long ret into 64-bits mode.
    mov eax, offset _BiTransferExecution$Finish
    push 28h
    push eax
    retf

_BiTransferExecution$Finish:
    mov ax, 10h
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Grab the return address.
    db 48h, 83h, 0C4h, 08h ; add rsp, 8
    pop eax

    ; Setup a proper aligned stack (16-bytes alignemnt after JMP), and jump execution.
    db 48h, 0C7h, 0C4h, 00h, 7Ch, 00h, 00h
    jmp eax
_BiTransferExecution endp
