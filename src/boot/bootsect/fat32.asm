; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: BSD-3-Clause

.model tiny
.386p

_TEXT segment use16
org 7C00h

; Dummy FAT32 BPB (128MiB hard disk), don't try actually using it.
FatBpb label byte
    SkipBpb db 0EBh, 58h, 90h
    OemIdent db "MSWIN4.1"
    BytesPerSector dw 512
    SectorsPerCluster db 1
    ReservedSectors dw 32
    NumOfFats db 2
    DirEntries dw 0
    NumOfSectors dw 0
    DescType db 0F8h
    SectorsPerFat dw 0
    SectorsPerTrack dw 32
    NumOfHeads dw 8
    HiddenSectors dd 0
    LargeSectors dd 40000h
    SectorsPerFat32 dd 07E1h
    Flags dw 0
    Version dw 0
    RootCluster dd 2
    InfoSector dw 1
    BackupSector dw 6
    Reserved0 db 12 dup (0)
    BootDrive db 0
    Reserved1 db 0
    Signature db 29h
    DataStart dd 0
    VolLabel db "BOOT DISK  "
    SysIdent db "FAT32   "

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
    ; Init the segments that we might use (to zero), the stack (so that the top coincides with the boot sector load
    ; address), and save the boot drive.
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 7C00h

    push dx
    mov [BootDrive], dl

    ; There is no need to calculate the start or the size of the root directory, so everything we need is the
    ; start of the data section clusters (which is the same as the position of the root dir in FAT12/16,
    ; fats + reserved).
    movzx eax, [NumOfFats]
    movzx ecx, [ReservedSectors]
    mul [SectorsPerFat32]
    add eax, ecx
    add eax, [HiddenSectors]
    mov [DataStart], eax

    ; Now we need to find the correct directory entry, loading a new cluster after we reach the end of the
    ; current one.
    mov ebp, [RootCluster]

Main$NextCluster:
    mov bx, 600h
    mov si, bx
    call ReadCluster

Main$Search:
    cmp byte ptr [si], 0
    je Main$NotFound

    test byte ptr [si + 11], 10h
    jnz Main$NextEntry

    push si
    mov cx, 11
    mov di, offset ImageName
    repe cmpsb
    pop si
    je Main$Found
    add si, 32
    cmp si, 512
    jne Main$Search

Main$NextEntry:
    call GetNextCluster
    jc Main$NextCluster

Main$NotFound:
    mov si, offset ImageError
    jmp Error

Main$Found:
    ; Now we can load all of the file data just after the boot sector.
    ; The only fields that we use from the directory entry are the ones at +20 (high bits of the cluster) and
    ; at +26 (low bits of the cluster).
    movzx ebx, word ptr [si + 26]
    movzx ebp, word ptr [si + 20]
    shl ebp, 16
    or ebp, ebx

    mov bx, 4000h
    mov es, bx
    xor bx, bx

Main$Loop:
    call ReadCluster
    call GetNextCluster
    jc Main$Loop

    pop dx
    push 4000h
    push 0
    retf
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
    loop ReadSectors
    ret
ReadSectors endp

ReadCluster proc
;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function converts the cluster number into an LBA/sector number and reads in the data (getting the next
;     cluster should be done with GetNextCluster).
;
; PARAMETERS:
;     Cluster (ebp) - Cluster to read.
;
; RETURN VALUE:
;     Same as ReadSectors.
;---------------------------------------------------------------------------------------------------------------------

    movzx ecx, [SectorsPerCluster]
    lea eax, [ebp - 2]
    mul ecx
    add eax, [DataStart]
    call ReadSectors
    ret
ReadCluster endp

GetNextCluster proc
;---------------------------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function loads the required FAT to get the next cluster of a chain (or to indicate that the file ended).
;     Afterwards, it loads the new/next cluster if possible/necessary.
;
; PARAMETERS:
;     Cluster (ebp) - Current cluster.
;
; RETURN VALUE:
;     cf will be unset if the file has ended, otherwise, new/next cluster on ebp.
;---------------------------------------------------------------------------------------------------------------------

    ; In the FAT, the size of each entry (representing a cluster) is 4 bytes. We can get the sector of the current
    ; cluster by doing Cluster*EntrySize/BytesPerSector, which will as a side effect leave the offset in the sector
    ; inside edx (because div).
    movzx ecx, [BytesPerSector]
    xor edx, edx
    mov eax, ebp
    shl eax, 2
    div ecx

    movzx ecx, [ReservedSectors]
    add eax, ecx

    push bx
    mov bx, es
    push es
    xor bx, bx
    mov es, bx
    mov bx, 600h
    mov cx, 1
    call ReadSectors
    pop bx
    mov es, bx
    pop bx

    mov ebp, [edx + 600h]
    and ebp, 0FFFFFFFh
    cmp ebp, 0FFFFFF0h
    ret
GetNextCluster endp

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

DiskError db "Disk error", 0
OverflowError db "Data overflow", 0
ImageError db "BOOTMGR is missing", 0
ImageName db "BOOTMGR    "

.errnz ($ - FatBpb) gt 510
org 7DFEh
dw 0AA55h

_TEXT ends
end
