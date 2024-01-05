; SPDX-FileCopyrightText: (C) 2023 ilmmatias
; SPDX-License-Identifier: GPL-3.0-or-later

.model tiny
.386p

_TEXT segment use16
org 7C00h

; Dummy exFAT BPB (64MiB hard disk), don't try actually using it.
FatBpb label byte
    JumpBoot db 0EBh, 76h, 90h
    FileSystemName db "EXFAT   "
    NumOfHeads dw 0
    SectorsPerTrack dw 0
    BytesPerSector dw 0
    SectorsPerCluster dd 0
    DataStart dd 0
    FileFlags db 0
    MustBeZero db 38 dup (0)
    PartitionOffset dq 0
    VolumeLength dq 131072
    FatOffset dd 128
    FatLength dd 128
    ClusterHeapOffset dd 256
    ClusterCount dd 16352
    FirstClusterOfRootDirectory dd 5
    VolumeSerialNumber dd 0
    FileSystemRevision dw 10h
    VolumeFlags dw 0
    BytesPerSectorShift db 9
    SectorsPerClusterShift db 3
    NumberOfFats db 1
    BootDrive db 80h
    PercentInUse db 0
    Reserved db 7 dup (0)

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
    ; Unknown register states are not cool, get all the segment registers ready, and setup a valid
    ; stack (ending behind us).
    cld
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 7C00h

    push dx
    mov [BootDrive], dl

    inc ax
    mov cl, [BytesPerSectorShift]
    shl ax, cl
    mov [BytesPerSector], ax

    mov eax, 1
    mov cl, [SectorsPerClusterShift]
    shl eax, cl
    mov [SectorsPerCluster], eax

    ; The exFAT code is a bit too big for just 512 bytes, but exFAT does reserve 8 extra sectors
    ; for us too, we just need to load them in.
    mov eax, 1
    add eax, dword ptr [PartitionOffset]
    mov bx, 07E00h
    mov cx, 1
    call ReadSectors

    ; Close to equal to FAT32, we also just need to grab the offset (in sectors) of the first
    ; cluster, and then the BPB gives us the first root directory cluster for free.
    mov eax, [ClusterHeapOffset]
    add eax, dword ptr [PartitionOffset]
    mov [DataStart], eax

    mov ebp, [FirstClusterOfRootDirectory]

    ; First read is different as we need to store the NoFatChain flag.
    mov bx, 600h
    mov si, bx
    clc
    call ReadCluster
    mov al, [si + 33]
    mov [FileFlags], al
    xor edi, edi
    jmp Main$Search

Main$NextCluster:
    mov bx, 600h
    mov si, bx
    xor edi, edi
    clc
    call ReadCluster

Main$Search:
    cmp byte ptr [si], 0
    je Main$NotFound

    ; We want a file (not a directory). We don't care about any other flags.
    cmp byte ptr [si], 85h
    jne Main$NextEntry
    test byte ptr [si + 4], 10h
    jnz Main$NextEntry

    ; We need exactly 2 entries to exist (stream entry + name entry), and only 2, as we know the
    ; name should fit in just 15 characters.
    cmp byte ptr [si + 1], 2
    jne Main$NextEntry
    cmp byte ptr [si + 32], 0C0h
    jne Main$NextEntry
    cmp byte ptr [si + 64], 0C1h
    jne Main$NextEntry

    ; Now we're safe to check the name length and bytes.
    cmp byte ptr [si + 35], ImageSize
    jne Main$NextEntry

    push si
    push edi
    mov cx, ImageSize
    add si, 66
    mov di, offset ImageName

Main$CheckName:
    ; exFAT is supposed to be case-insensitive, but it does save the names using the full UTF-16
    ; range (so it might include lowercase characters).
    ; We're supposed to load and use the uppercase table, but let's just manually check and convert.
    cmp word ptr [si], 61h
    jb Main$NotLower
    cmp word ptr [si], 7Ah
    ja Main$NotLower
    sub word ptr [si], 32
Main$NotLower:
    cmpsw
    jne Main$EndCheck
    loop Main$CheckName

Main$EndCheck:
    pop edi
    pop si
    je Main$Found

Main$NextEntry:
    add si, 32
    call CheckClusterBoundaries
    jc Main$Search
    test byte ptr [FileFlags], 2
    call GetNextCluster
    jc Main$NextCluster

Main$NotFound:
    mov si, offset ImageError
    jmp Error

Main$Found:
    ; Now we can load all of the file data just after the boot sector, and jump to it.
    mov ebp, [si + 52]
    mov bx, 920h
    mov es, bx
    xor bx, bx

    stc
    call ReadCluster
    mov al, [si + 33]
    mov [FileFlags], al
    jmp Main$LoopSkipRead

Main$Loop:
    call ReadCluster
Main$LoopSkipRead:
    test byte ptr [FileFlags], 2
    call GetNextCluster
    jc Main$Loop

    pop dx
    push 920h
    push 0
    retf
Main endp

;--------------------------------------------------------------------------------------------------
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
;--------------------------------------------------------------------------------------------------
ReadSectors proc
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
    loop ReadSectors
    ret
ReadSectors endp

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

DiskError db "Disk error", 0
OverflowError db "Data error", 0

.errnz ($ - FatBpb) gt 510
org 7DFEh
dw 0AA55h

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function converts the cluster number into an LBA/sector number and reads in the data
;     (getting the next cluster should be done with GetNextCluster).
;
; PARAMETERS:
;     Cluster (ebp) - Cluster to read.
;     ReadWhole (flags & CF) - Set to 1 if we should read the whole cluster, to 0 if we should
;                              only read a single sector.
;
; RETURN VALUE:
;     Same as ReadSectors.
;--------------------------------------------------------------------------------------------------
ReadCluster proc
    jnc ReadCluster$ReadPartial
    mov ecx, [SectorsPerCluster]
    lea eax, [ebp - 2]
    mul ecx
    add eax, [DataStart]
    call ReadSectors
    ret

ReadCluster$ReadPartial:
    mov ecx, [SectorsPerCluster]
    lea eax, [ebp - 2]
    mul ecx
    add eax, [DataStart]
    mov ecx, 1
    call ReadSectors
    ret
ReadCluster endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function validates if we're still within the current sector, increasing the current
;     sector if required.
;
; PARAMETERS:
;     CurrentOffset (si) - Offset across the current sector.
;     CurrentSector (edi) - How many sectors deep are we.
;     Cluster (ebp) - Cluster we're currently in.
;
; RETURN VALUE:
;     Flags will be set so that `jc` will NOT JUMP if we reached the end of the cluster.
;--------------------------------------------------------------------------------------------------
CheckClusterBoundaries proc
    mov ax, si
    sub ax, 600h
    cmp ax, [BytesPerSector]
    jae CheckClusterBoundaries$EndOfSector
    stc
    ret

CheckClusterBoundaries$EndOfSector:
    inc edi
    cmp edi, [SectorsPerCluster]
    jb CheckClusterBoundaries$ReadNextSector
    clc
    ret

CheckClusterBoundaries$ReadNextSector:
    mov ecx, [SectorsPerCluster]
    lea eax, [ebp - 2]
    mul ecx
    add eax, [DataStart]
    add eax, edi
    mov ecx, 1
    call ReadSectors
    stc
    ret
CheckClusterBoundaries endp

;--------------------------------------------------------------------------------------------------
; PURPOSE:
;     This function loads the required FAT to get the next cluster of a chain (or to indicate that
;     the file ended).
;     Afterwards, it loads the new/next cluster if possible/necessary.
;
; PARAMETERS:
;     DirectoryEntry (si) - Directory entry (used to determine if we need to follow the FAT).
;     Cluster (ebp) - Current cluster.
;     NoFatChain (!(flags & ZF)) - Unset the ZF flag to follow the chain, set it if we're
;                                  sequential.
;
; RETURN VALUE:
;     Flags will be set so that `jc` will NOT JUMP if the file has ended, otherwise, new/next
;     cluster  on ebp.
;--------------------------------------------------------------------------------------------------
GetNextCluster proc
    ; exFAT supports the sequential data sectors (no need to read the FAT in that case, just
    ; decrease the file size and check if we finished reading, that is, if the size is negative).
    jz GetNextCluster$FollowFat

    mov eax, 1
    movzx ecx, [SectorsPerClusterShift]
    shl eax, cl
    movzx ecx, [BytesPerSectorShift]
    shl eax, cl

    inc ebp
    sub [si + 56], eax
    stc
    jns GetNextCluster$NoSign
    clc
GetNextCluster$NoSign:
    ret

GetNextCluster$FollowFat:
    ; In the FAT, the size of each entry (representing a cluster) is 4 bytes. We can get the
    ; sector of the current cluster by doing Cluster*EntrySize/BytesPerSector, which will as a
    ; side effect leave the offset in the sector inside edx (because div).
    movzx ecx, [BytesPerSector]
    xor edx, edx
    mov eax, ebp
    shl eax, 2
    div ecx

    add eax, [FatOffset]
    add eax, dword ptr [PartitionOffset]

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
    cmp ebp, 0FFFFFFF6h
    ret
GetNextCluster endp

ImageError db "BOOTMGR is missing", 0
ImageName dw 'B', 'O', 'O', 'T', 'M', 'G', 'R'
ImageSize equ ($ - ImageName) / 2

.errnz ($ - FatBpb) gt 1020
org 7FFCh
dd 0AA550000h

_TEXT ends
end
