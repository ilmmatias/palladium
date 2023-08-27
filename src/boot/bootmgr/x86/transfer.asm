; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.code

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function puts the processor in long mode, and transfers execution away to the kernel.
;
; PARAMETERS:
;     Pml4 (esp + 4) - Physical address of the PML4 structure.
;     BootData (esp + 8) - Virtual address (64-bits) where the loader boot data is located.
;     Images (esp + 16) - Virtual address (64-bits) where the loaded images array begins.
;     ImageCount (esp + 24) - How many boot images were loaded.
;
; RETURN VALUE:
;     Does not return.
;--------------------------------------------------------------------------------------------------
_BiTransferExecution proc
    ; Enable long mode and no-execute (for W^X) in the EFER.
    mov ecx, 0C0000080h
    rdmsr
    or eax, 900h
    wrmsr

    ; Enable PAE (physical address extension); Required for long mode.
    mov eax, cr4
    or eax, 20h
    mov cr4, eax

    ; Enable the paging flag; CR3 is set first, otherwise we'll page fault trying to resolve the
    ; current address.
    mov eax, [esp + 4]
    mov cr3, eax
    mov eax, cr0
    or eax, 80010000h
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
    mov ss, ax

    ; Skip the return address and Pml4, and start up from the 2nd entry in the image list.
    ; The 1st should be the kernel, which is loaded at the end.
    db 48h, 83h, 0C4h, 08h ; add rsp, 8
    pop ecx
    pop edi
    pop esi

    ; Setup a proper aligned stack (16-bytes alignemnt after CALL) for all the entry points.
    db 48h, 0C7h, 0C4h, 0F8h, 7Bh, 00h, 00h ; mov rsp, 7BF8h (0x7C00 - 8)
    push edi
    push ecx
_BiTransferExecution$CallDriver:
    db 48h, 0FFh, 0CEh ; dec rsi
    jz _BiTransferExecution$CallKernel

    db 48h, 83h, 0C7h, 24h ; add rdi, 36 (sizeof LoadedImage)
    db 48h, 8Bh, 6Fh, 18h ; mov rbp, [rdi + 24]
    call ebp

    jmp _BiTransferExecution$CallDriver

_BiTransferExecution$CallKernel:
    pop ecx
    pop edi
    db 48h, 8Bh, 6Fh, 18h ; mov rbp, [rdi + 24]
    call ebp

_BiTransferExecution$Loop:
    jmp _BiTransferExecution$Loop
_BiTransferExecution endp
