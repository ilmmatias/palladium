; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.model tiny
.386p

_TEXT segment use16
org 7C00h

; Dummy NTFS BPB (64MiB hard disk), don't try actually using it.
NtfsBpb label byte
    JumpBoot db 0EBh, 52h, 90h
    FileSystemName db "NTFS    "
    BytesPerSector dw 512
    RawSectorsPerCluster db 8
    SectorsPerCluster dd 0
    Reserved db 0
    NumberOfSectors16 dw 0
    Media db 0F8h
    BytesPerCluster dd 0
    NumberOfHeads dw 0
    SectorsPerMftEntry dd 0
    BytesPerMftEntry dd 0
    BootDrive db 80h
    BpbFlags db 0
    BpbSignature db 80h
    Reserved2 db 0
    NumberOfSectors dq 131072
    MftCluster dq 4
    MirrorMftCluster dq 8191
    MftEntrySize db 0F6h
    Reserved3 db 0, 0, 0
    IndexEntrySize db 1
    Reserved4 db 0, 0, 0
    MftOffset dq 0
    Checksum dd 0

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

    ; For SpC > 243, the actual amount of sectors is 2^(256 - SpC).
    xor ebx, ebx
    mov bl, [RawSectorsPerCluster]
    cmp bl, 243
    mov eax, ebx
    jbe Main$CalculateMftEntrySize

Main$PowClusters:
    mov ecx, 256
    sub ecx, ebx
    mov eax, 1
    shl eax, cl

Main$CalculateMftEntrySize:
    mov [SectorsPerCluster], eax
    movzx edx, [BytesPerSector]
    imul eax, edx
    mov [BytesPerCluster], eax

    ; Simillar rule, but for negative numbers, we're just taking 2^(-MeS).
    xor eax, eax
    mov al, [MftEntrySize]
    cmp al, 127
    ja Main$PowMftEntrySize

    imul eax, [SectorsPerCluster]
    jmp Main$LoadMoreSectors

Main$PowMftEntrySize:
    neg al
    mov cl, al
    mov eax, 1
    shl eax, cl
    xor edx, edx
    div [BytesPerSector]

Main$LoadMoreSectors:
    mov [SectorsPerMftEntry], eax
    movzx edx, [BytesPerSector]
    imul eax, edx
    mov [BytesPerMftEntry], eax

    mov eax, dword ptr [MftCluster]
    mov edx, dword ptr [MftCluster + 4]
    mov ebx, [SectorsPerCluster]
    call Multiply64x32
    mov dword ptr [MftOffset], eax
    mov dword ptr [MftOffset + 4], edx

    ; NTFS is a beast of a FS, even for just finding a file in the root directory.
    ; We're forced to use more sectors, which fortunately, we have 15 spare.
    mov eax, 1
    xor edx, edx
    mov bx, 07E00h
    mov ecx, 1
    call ReadSectors

    jmp LoadBootmgr
Main endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function reads sectors from the boot disk. The input should be an LBA value.
;
; PARAMETERS:
;     FirstSector (eax:edx) - Which sector to start reading from.
;     Buffer (es:bx) - Destination of the read data.
;     SectorCount (ecx) - How many sectors to read.
;
; RETURN VALUE:
;     Does not return on failure, all of the inputs are already incremented on success.
;---------------------------------------------------------------------------------------------------------------------
ReadSectors proc
    pushad

    ; We just need to mount the disk address packet and call int 13h.
    db 66h
    push edx
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
    add bx, [BytesPerSector]
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
    dec ecx
    jnz ReadSectors
    ret
ReadSectors endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function multiplies the 64-bits number at EAX:EDX with the 32-bits number at EBX.
;
; PARAMETERS:
;     Left (eax:edx) - Left side of the expression.
;     Right (ebx) - Right side of the expression.
;
; RETURN VALUE:
;     Result of the multiplication in EAX:EDX.
;---------------------------------------------------------------------------------------------------------------------
Multiply64x32 proc
    push esi
    mov esi, edx

    ; B * A
    mul ebx
    ; B * D * 2^32
    imul esi, ebx
    ; B * A + B * D * 2^32
    add edx, esi

    pop esi
    ret
Multiply64x32 endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function multiplies the 64-bits number at EAX:EDX with the 64-bits number at EBX:ECX.
;
; PARAMETERS:
;     Left (eax:edx) - Left side of the expression.
;     Right (ebx:ecx) - Right side of the expression.
;
; RETURN VALUE:
;     Result of the multiplication in EAX:EDX.
;---------------------------------------------------------------------------------------------------------------------
Multiply64x64 proc
    push esi
    push edi
    mov esi, eax
    mov edi, edx

    ; B * A
    mul ebx
    ; B * D * 2^32
    imul edi, ebx
    ; C * A * 2^32
    imul esi, ecx
    ; B * D:A + C * A * 2^32
    add edx, esi
    add edx, edi

    pop edi
    pop esi
    ret
Multiply64x64 endp

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

DiskError db "Disk error", 0
OverflowError db "Data overflow", 0
FsCorruptionError db "Bad filesystem", 0

.errnz ($ - NtfsBpb) gt 510
org 7DFEh
dw 0AA55h

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the main code to parse the NTFS structure, search for the file, and load it into RAM.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     Does not return.
;---------------------------------------------------------------------------------------------------------------------
LoadBootmgr proc
    ; The 5th MFT entry is `.` (the root directory).
    mov ecx, [SectorsPerMftEntry]
    mov eax, 5
    xor edx, edx
    mov ebx, ecx
    call Multiply64x32
    add eax, dword ptr [MftOffset]
    adc edx, dword ptr [MftOffset + 4]
    ; There should be enough space for a cluster/MFT entry, as long as the FS isn't corrupt/bogus.
    mov bx, 600h
    call ReadSectors

    ; We're doing the bare minimum to be able to boot here, and fixups are part of that, as otherwise part of the
    ; sectors would be corrupted/contain wrong data.
    call ApplyFixups

    ; $I30 ($INDEX_ROOT and $INDEX_ALLOCATION) are the attributes containing the directory
    ; tree itself (that we can iterate over).
    mov eax, 90h
    call FindAttribute
    jc LoadBootmgr$Corrupt

LoadBootmgr$Corrupt:
    mov si, offset FsCorruptionError
    jmp Error
LoadBootmgr endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function applies the fixups for any given record.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     None.
;---------------------------------------------------------------------------------------------------------------------
ApplyFixups proc
    push eax
    push esi
    push edi
    push ecx

    ; We're applying the fixups to the last 2 bytes of each sector.
    mov si, 5FEh
    add si, [BytesPerSector]

    ; Load up the mount of fixups.
    mov di, 606h
    mov cx, [di]

    ; Load up the fixup array offset.
    mov di, 602h
    add di, [di + 2]
ApplyFixups$Loop:
    cmp cx, 1
    jbe ApplyFixups$End

    mov ax, [di]
    mov [si], ax
    add si, [BytesPerSector]
    add di, 2

    dec cx
    jmp ApplyFixups$Loop

ApplyFixups$End:
    pop ecx
    pop edi
    pop esi
    pop eax
    ret
ApplyFixups endp

;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function searches for a given attribute inside a FILE record.
;
; PARAMETERS:
;     Type (eax) - Attribute's `Type` field to search for.
;     Buffer (bp) - Output; Pointer to the attribute data.
;
; RETURN VALUE:
;     Carry flag set if we failed to find the entry, carry unset with Buffer properly set otherwise.
;---------------------------------------------------------------------------------------------------------------------
FindAttribute proc
    push esi
    push edi

    ; Load up the attribute list.
    mov si, 600h
    add si, [si + 14h]

FindAttribute$Loop:
    ; Check for overflow; That means either the FS is corrupt/not really NTFS, or we really
    ; didn't find the attribute.
    mov di, si
    sub di, 600h
    cmp di, word ptr [BytesPerMftEntry]
    jae FindAttribute$End

    cmp dword ptr [si], eax
    jne FindAttribute$Mismatch

    ; Resident attributes are already loaded and we have nothing else to do.
    test byte ptr [si + 8], 1
    jnz FindAttribute$NonResident

    mov bp, si
    add bp, word ptr [si + 20]

    pop edi
    pop esi
    clc
    ret

FindAttribute$NonResident:
    jmp $

FindAttribute$Mismatch:
    ; Type mismatch, continue searching on the next entry.
    add si, [si + 4]
    jmp FindAttribute$Loop

FindAttribute$End:
    pop edi
    pop esi
    stc
    ret
FindAttribute endp

.errnz ($ - NtfsBpb) gt 1024

_TEXT ends
end
