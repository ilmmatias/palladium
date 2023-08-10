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
    NonResident db 0
    DataRuns dw 0
    Media db 0F8h
    BytesPerCluster dd 0
    NumberOfHeads dw 0
    SectorsPerMftEntry dd 0
    BytesPerMftEntry dd 0
    BootDrive db 80h
    BpbFlags db 0
    BpbSignature db 80h
    Reserved2 db 0
    DataRunLength dq 131072
    MftCluster dq 4
    DataRunOffset dq 8191
    MftEntrySize db 0F6h
    Reserved3 db 0, 0, 0
    IndexEntrySize db 1
    Reserved4 db 0, 0, 0
    MftOffset dq 0
    Checksum dd 0

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the necessary code to load the second stage loader and execute it.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     Does not return.
;--------------------------------------------------------------------------------------------------
Main proc
    ; Some BIOSes might load us to 07C0:0000, but our code expects the cs to be 0 (so the BPB
    ; would have been loaded to 0000:7C00), so let's do a far jump just to make sure.
    db 0EAh
    dw Main$Setup, 0

Main$Setup:
    ; Unknown register states are not cool, get all the segment registers ready, and setup a
    ; valid stack (ending behind us).
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
    mov ecx, 2
    call ReadSectors
    jmp LoadBootmgr
Main endp

;--------------------------------------------------------------------------------------------------
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
;--------------------------------------------------------------------------------------------------
ReadSectors proc
    pushad

    ; We just need to mount the disk address packet and call int 13h.
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

    add sp, 16
    popad
    jnc ReadSectors$Next
    mov si, offset DiskError
    jmp Error

ReadSectors$Next:
    inc eax
    add bx, [BytesPerSector]
    jnc ReadSectors$NoOverflow

    ; We overflowed in the low 16-bits of the address.
    ; Let's try to bump up the high 4 bits (segment), if we also overflowed there, then we crash
    ; (data too big for us).

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

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function multiplies the 64-bits number at EAX:EDX with the 32-bits number at EBX.
;
; PARAMETERS:
;     Left (eax:edx) - Left side of the expression.
;     Right (ebx) - Right side of the expression.
;
; RETURN VALUE:
;     Result of the multiplication in EAX:EDX.
;--------------------------------------------------------------------------------------------------
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

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function multiplies the 64-bits number at EAX:EDX with the 64-bits number at EBX:ECX.
;
; PARAMETERS:
;     Left (eax:edx) - Left side of the expression.
;     Right (ebx:ecx) - Right side of the expression.
;
; RETURN VALUE:
;     Result of the multiplication in EAX:EDX.
;--------------------------------------------------------------------------------------------------
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

ResidentMessage db "Resident", 0
NonResidentMessage db "Non Resident", 0
DiskError db "Disk error", 0
OverflowError db "Data overflow", 0
FsCorruptionError db "Bad filesystem", 0
ImageError db "BOOTMGR is missing", 0
ImageName dw 'B', 'O', 'O', 'T', 'M', 'G', 'R'
ImageSize equ ($ - ImageName) / 2

.errnz ($ - NtfsBpb) gt 510
org 7DFEh
dw 0AA55h

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function implements the main code to parse the NTFS structure, search for the file,
;     and load it into RAM.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     Does not return.
;--------------------------------------------------------------------------------------------------
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
    mov bx, 1000h
    mov es, bx
    xor bx, bx
    call ReadSectors

    mov bx, 1000h
    mov es, bx
    xor bx, bx

    ; We're doing the bare minimum to be able to boot here, and fixups are part of that, as
    ; otherwise part of the sectors would be corrupted/contain wrong data.
    call ApplyFixups

    ; $I30 ($INDEX_ROOT and $INDEX_ALLOCATION) are the attributes containing the directory
    ; tree itself (that we can iterate over).
    mov eax, 90h
    call FindAttribute
    jc LoadBootmgr$Corrupt

    add di, 16
    call TraverseIndexBlock
    jnc LoadBootmgr$Found
    jz LoadBootmgr$NotFound

    ; Failed to find it in the INDEX_ROOT, but it has sub blocks; We can assume that means
    ; an INDEX_ALLOCATION attribute exists.
    mov eax, 0A0h
    call FindAttribute
    jc LoadBootmgr$Corrupt

    mov bx, 2000h
    mov es, bx
    xor bx, bx

LoadBootmgr$TraverseBlock:
    xor di, di
    call ApplyFixups

    add di, 24
    call TraverseIndexBlock
    jnc LoadBootmgr$Found
    jz LoadBootmgr$NotFound

LoadBootmgr$Found:
    ; EBX:ECX contains the file size, but we use those registers for the multiplication below.
    push ebx
    push ecx

    ; EAX:EDX contains our MFT entry now; The process to load it is the same as the root
    ; directory.
    mov ecx, [SectorsPerMftEntry]
    mov ebx, ecx
    call Multiply64x32
    add eax, dword ptr [MftOffset]
    adc edx, dword ptr [MftOffset + 4]

    mov bx, 1000h
    mov es, bx
    xor bx, bx
    call ReadSectors

    mov bx, 1000h
    mov es, bx

    pop ecx
    pop ebx

    ; $DATA contains the unnamed data stream; aka, contains the main file contents.
    call ApplyFixups
    mov eax, 080h
    call FindAttribute
    jc LoadBootmgr$Corrupt

    ; We're limited to the high 230ish KiB of the low memory.
    setz [NonResident]
    cmp ecx, 0
    jne LoadBootmgr$Overflow
    cmp ebx, 38000h
    ja LoadBootmgr$Overflow

    mov bp, 4000h
    mov es, bp
    mov si, di
    xor di, di

    ; Resident data can just be rep movsb'd in a single go (no more clusters to handle).
    cmp [NonResident], 1
    je LoadBootmgr$LoadNonResident

    mov bp, es
    mov ds, bp
    call CopyLargeBuffer
    jmp LoadBootmgr$End

LoadBootmgr$LoadNonResident:
    ; Either copy a whole cluster, or however many bytes are left.
    cmp ebx, [BytesPerCluster]
    jb LoadBootmgr$UseRemainingSize
    mov ecx, [BytesPerCluster]
    jmp LoadBootmgr$CopyCluster
LoadBootmgr$UseRemainingSize:
    mov ecx, ebx

LoadBootmgr$CopyCluster:
    ; We'll always be loading the cluster to the temp buffer (right at the start); So, always
    ; reset the source.
    mov bp, 2000h
    mov ds, bp
    xor bp, bp
    mov si, bp

    push ebx
    push ecx
    xchg ecx, ebx
    call CopyLargeBuffer
    pop ecx
    pop ebx

    sub ebx, ecx
    jz LoadBootmgr$End

    ; The virtual clusters are sequential, but we do need to convert them into logical clusters
    ; first, followed by converting into sectors.
    inc eax
    adc edx, 0
    push eax
    push edx
    call TranslateVcn

    ; We need to handle ReadSectors() trashing most/all of the registers we use.
    pushad
    push es
    xor di, di
    mov ds, di
    mov di, 2000h
    mov es, di
    xor bx, bx
    mov ecx, [SectorsPerCluster]
    call ReadSectors
    pop es
    popad

    pop edx
    pop eax
    jmp LoadBootmgr$LoadNonResident

LoadBootmgr$End:
    pop dx
    push 4000h
    push 0
    retf

LoadBootmgr$NotFound:
    mov si, offset ImageError
    jmp Error

LoadBootmgr$Corrupt:
    mov si, offset FsCorruptionError
    jmp Error

LoadBootmgr$Overflow:
    mov si, offset OverflowError
    jmp Error
LoadBootmgr endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function applies the fixups for any given record.
;
; PARAMETERS:
;     None.
;
; RETURN VALUE:
;     None.
;--------------------------------------------------------------------------------------------------
ApplyFixups proc
    push eax
    push esi
    push edi
    push ecx

    ; We're applying the fixups to the last 2 bytes of each sector.
    mov si, [BytesPerSector]
    sub si, 2

    ; Load up the mount of fixups.
    mov di, 6
    mov cx, es:[di]

    ; Load up the fixup array offset.
    mov di, 2
    add di, es:[di + 2]
ApplyFixups$Loop:
    cmp cx, 1
    jbe ApplyFixups$End

    mov ax, es:[di]
    mov es:[si], ax
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

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function searches for a given attribute inside a FILE record.
;
; PARAMETERS:
;     Type (eax) - Attribute's `Type` field to search for.
;     Buffer (di) - Output; Pointer to the attribute data, for the resident form.
;
; RETURN VALUE:
;     Carry flag set if we failed to find the entry.
;     Zero flag set if this is a non resident entry (and we should ignore the buffer); In that
;     case, EAX:EDX contains the starting VCN of the attribute.
;--------------------------------------------------------------------------------------------------
FindAttribute proc
    push ebx
    push esi

    ; Load up the attribute list.
    xor si, si
    add si, es:[si + 20]

FindAttribute$Loop:
    ; Check for overflow; That means either the FS is corrupt/not really NTFS, or we really
    ; didn't find the attribute.
    cmp di, word ptr [BytesPerMftEntry]
    jae FindAttribute$End

    cmp dword ptr es:[si], eax
    jne FindAttribute$Mismatch

    ; Resident attributes are already loaded and we have nothing else to do.
    test byte ptr es:[si + 8], 1
    jnz FindAttribute$NonResident

    mov di, si
    add di, es:[si + 20]

    pop esi
    pop ebx
    test sp, sp
    clc
    ret

FindAttribute$NonResident:
    ; Non resident clusters get loaded on a separate buffer, as we need access to the data run
    ; list at all times.
    mov di, si
    add di, es:[si + 32]
    mov [DataRuns], di

    mov [Reserved2], al
    mov eax, es:[si + 16]
    mov edx, es:[si + 20]
    push eax
    push edx
    call TranslateVcn

    push es
    mov di, 2000h
    mov es, di
    xor bx, bx
    mov ecx, [SectorsPerCluster]
    call ReadSectors
    pop es

    pop edx
    pop eax
    pop esi
    pop ebx
    cmp esi, esi
    clc
    ret

FindAttribute$Mismatch:
    ; Type mismatch, continue searching on the next entry.
    add si, es:[si + 4]
    jmp FindAttribute$Loop

FindAttribute$End:
    pop esi
    pop ebx
    stc
    ret
FindAttribute endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function parses the data run list, and converts a Virtual Cluster Number (VCN) into a
;     physical sector.
;
; PARAMETERS:
;     Vcn (eax:edx) - Virtual cluster to be translated.
;
; RETURN VALUE:
;     Physical sector where VCN is located (overwriting the old value in eax:edx).
;--------------------------------------------------------------------------------------------------
TranslateVcn proc
    push ebx
    push ecx
    push ebp
    push esi
    push edi
    push ds
    push es

    ; We should only be opening one file/directory at a time, so assuming [DataRuns] contains
    ; the revelant data is safe.
    xor si, si
    mov ds, si
    mov si, 1000h
    mov es, si
    mov si, [DataRuns]

    xor ebp, ebp
    xor ecx, ecx
TranslateVcn$Loop:
    mov bl, es:[si]
    test bl, bl
    jz TranslateVcn$End
    inc si

    ; rep movsb copies whatever is at DS:SI to ES:DI; That means we're almost set, but our
    ; segment registers are inverted.
    push ecx
    push es
    mov cx, ds
    mov es, cx
    pop ds

    ; We shouldn't be over 8 bytes, if we are, we're just discarding those higher bits.
    movzx cx, bl
    and cx, 0Fh
    mov di, offset DataRunLength
    rep movsb

    movzx cx, bl
    shr cx, 4
    mov di, offset DataRunOffset
    rep movsb

    push es
    mov cx, ds
    mov es, cx
    pop ds
    pop ecx

    cmp edx, ecx
    jb TranslateVcn$Next
    cmp eax, ebp
    jb TranslateVcn$Next

    ; VCN match, extract the right cluster out of it, and Multiply64x32 to transform it
    ; into physical sectors.
    sub eax, ebp
    sbb edx, ecx
    add eax, dword ptr [DataRunOffset]
    adc edx, dword ptr [DataRunOffset + 4]
    mov ebx, [SectorsPerCluster]
    call Multiply64x32
    jmp TranslateVcn$End

TranslateVcn$Next:
    add ebp, dword ptr [DataRunLength]
    adc ecx, dword ptr [DataRunLength + 4]
    jmp TranslateVcn$Loop

TranslateVcn$End:
    pop es
    pop ds
    pop edi
    pop esi
    pop ebp
    pop ecx
    pop ebx
    ret
TranslateVcn endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function searches for the image file inside an directory index block.
;
; PARAMETERS:
;     HeaderStart (es:di) - Where the index header starts.
;
; RETURN VALUE:
;     Carry flag set if we didn't find it; Otherwise, EAX:EDX points to file MFT entry, and
;     EBX:ECX points to the  file length.
;     On failure, zero flag set if this entry has no sub nodes; Otherwise, EAX:EDX points to the
;     sub node VCN.
;--------------------------------------------------------------------------------------------------
TraverseIndexBlock proc
    push ecx
    push ebp
    push esi

    ; 32-bits field, but we're assuming it is 16-bits only (as we can't access address too big
    ; in real mode).
    mov si, word ptr es:[di]
    add si, di

TraverseIndexBlock$Loop:
    ; Overflow validation; We'll take an overflow as `entry not found and no sub nodes`.
    mov bp, si
    sub bp, di
    cmp bp, word ptr es:[di + 4]
    jae TraverseIndexBlock$Overflow

    ; Check the end flag; If it's set, we know that it's the end of this block, but there might
    ; be sub blocks/nodes.
    test byte ptr es:[si + 12], 2
    jz TraverseIndexBlock$Compare

    ; Pre load the next VCN, as we don't want to lose the zero flag.
    mov di, si
    add si, es:[si + 8]
    sub si, 8
    mov eax, es:[si]
    mov edx, es:[si + 4]

    test byte ptr es:[di + 12], 1
    pop esi
    pop ebp
    pop ecx
    stc
    ret

TraverseIndexBlock$Compare:
    ; Don't even bother with directories; We should be searching a file.
    test byte ptr es:[si + 75], 10h
    jnz TraverseIndexBlock$Next

    ; We should have a perfect match for the name length, otherwise, this is certainly not our
    ; file.
    cmp byte ptr es:[si + 80], ImageSize
    jne TraverseIndexBlock$Next

    push si
    push di
    mov cx, ImageSize
    lea di, [si + 82]
    mov si, offset ImageName

TraverseIndexBlock$CheckName:
    ; NTFS stores the filenames in their original case, but we don't care (we just want to do
    ; a case insensitive compare).
    ; We're supposed to load and use the uppercase table, but let's just manually check and
    ; convert.
    cmp word ptr es:[di], 61h
    jb TraverseIndexBlock$NotLower
    cmp word ptr es:[di], 7Ah
    ja TraverseIndexBlock$NotLower
    sub word ptr es:[di], 32
TraverseIndexBlock$NotLower:
    cmpsw
    jne TraverseIndexBlock$EndCheck
    loop TraverseIndexBlock$CheckName
TraverseIndexBlock$EndCheck:
    pop di
    pop si
    jne TraverseIndexBlock$Next

    ; The MFT entry is only 48-bits, not the full uint64_t.
    mov eax, dword ptr es:[si]
    mov edx, dword ptr es:[si + 4]
    and edx, 0FFFFh

    mov ebx, dword ptr es:[si + 64]
    mov ecx, dword ptr es:[si + 68]

    pop esi
    pop ebp
    pop ecx
    clc
    ret

TraverseIndexBlock$Next:
    add si, es:[si + 8]
    jmp TraverseIndexBlock$Loop

TraverseIndexBlock$Overflow:
    pop esi
    pop ebp
    pop ecx
    stc
    cmp esi, esi
    ret
TraverseIndexBlock endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function extends `rep movsb` into multi-segment copies.
;
; PARAMETERS:
;     Buffer (ES:DI) - Destination address.
;     Source (DS:SI) - Source address.
;     Size (EBX) - Data size.
;
; RETURN VALUE:
;     All input parameters + ECX are trashed (including the two segments); Other than that, no
;     return value.
;--------------------------------------------------------------------------------------------------
CopyLargeBuffer proc
    cmp ebx, 1000h
    jbe CopyLargeBuffer$UseRemainingSize
    mov ecx, 1000h
    jmp CopyLargeBuffer$MainCopy
CopyLargeBuffer$UseRemainingSize:
    mov ecx, ebx

CopyLargeBuffer$MainCopy:
    push ecx
    push di
    push si
    rep movsb
    pop si
    pop di
    pop ecx

    ; We ignore the auto increment `movsb` does, as that doesn't handle overflow properly.
    push ebx
    add di, cx
    jnc CopyLargeBuffer$IncrementSource
    mov bx, es
    add bx, 1000h
    cmp bx, 0A000h
    ja CopyLargeBuffer$Overflow
    mov es, bx

CopyLargeBuffer$IncrementSource:
    add si, cx
    jnc CopyLargeBuffer$DecrementSize
    mov bx, ds
    add bx, 1000h
    cmp bx, 0A000h
    ja CopyLargeBuffer$Overflow
    mov ds, bx

CopyLargeBuffer$DecrementSize:
    pop ebx
    sub ebx, ecx
    jnz CopyLargeBuffer
    ret

CopyLargeBuffer$Overflow:
    mov si, offset OverflowError
    jmp Error
CopyLargeBuffer endp

.errnz ($ - NtfsBpb) gt 1536

_TEXT ends
end
