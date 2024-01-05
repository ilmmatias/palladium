/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <file.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <x86/bios.h>

typedef struct __attribute__((packed)) {
    uint16_t Size;
    uint16_t Flags;
    uint32_t Cylinders;
    uint32_t Heads;
    uint32_t SectorsPerTrack;
    uint64_t Sectors;
    uint16_t BytesPerSector;
    uint32_t EddPointer;
} ExtDriveParameters;

typedef struct __attribute__((packed)) {
    uint8_t Size;
    uint8_t AlwaysZero;
    uint16_t Sectors;
    uint16_t TransferOffset;
    uint16_t TransferSegment;
    uint64_t StartSector;
} ExtDrivePacket;

typedef struct {
    BmPartition Base;
    ExtDriveParameters Parameters;
} DiskContext;

extern BmPartition *BiBootPartition;

static char ReadBuffer[4096] __attribute__((aligned(16)));
static DiskContext DriveParameters[256] = {};
static RtSList DrivePartitions[256] = {};

uint8_t BiosBootDisk = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function uses the BIOS to read sectors from one of the detected disks.
 *
 * PARAMETERS:
 *     Context - Expected to be a valid drive parameter struct.
 *     Offset - Starting byte index (into the disk).
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ReadDisk(void *Context, uint64_t Offset, uint64_t Size, void *Buffer) {
    DiskContext *Drive = Context;
    char *OutputBuffer = Buffer;

    alignas(16) ExtDrivePacket Packet;
    BiosRegisters Registers;

    /* Always read sector-by-sector, as we can't really trust the Sectors field in the DAP (
       some BIOSes don't properly implement it). */
    while (Size) {
        memset(&Registers, 0, sizeof(BiosRegisters));
        Registers.Eax = 0x4200;
        Registers.Edx = Drive - DriveParameters;
        Registers.Esi = (uint32_t)&Packet;

        Packet.Size = 16;
        Packet.AlwaysZero = 0;
        Packet.Sectors = 1;
        Packet.TransferOffset = (uint16_t)(uint32_t)ReadBuffer;
        Packet.TransferSegment = (uint16_t)(((uint32_t)ReadBuffer & 0xF0000) >> 4);
        Packet.StartSector = Offset / Drive->Parameters.BytesPerSector;

        BiosCall(0x13, &Registers);
        if (Registers.Eflags & 1) {
            return 0;
        }

        size_t SourceOffset = Offset % Drive->Parameters.BytesPerSector;
        size_t CopySize = Drive->Parameters.BytesPerSector - SourceOffset;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(OutputBuffer, ReadBuffer + SourceOffset, CopySize);
        OutputBuffer += CopySize;
        Offset += CopySize;
        Size -= CopySize;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function uses the BIOS to detect all plugged in disks (that we can use extended
 *     functions).
 *
 * PARAMETERS:
 *     BootInfo - For us, this is the DL (EDX) value, containing the BIOS boot device.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiInitializeDisks(void *BootInfo) {
    BiosRegisters Registers;
    ExtDriveParameters Parameters;

    memset(&DriveParameters, 0, sizeof(DriveParameters));

    for (int i = 0; i < 256; i++) {
        /* Make sure extensions are present, and that packet access is supported (that's all
           we'll use, for the read function). */
        memset(&Registers, 0, sizeof(BiosRegisters));
        Registers.Eax = 0x4100;
        Registers.Edx = i;
        Registers.Ebx = 0x55AA;

        BiosCall(0x13, &Registers);
        if ((Registers.Eflags & 1) || Registers.Ebx != 0xAA55 || !(Registers.Ecx & 1)) {
            continue;
        }

        memset(&Registers, 0, sizeof(BiosRegisters));
        Registers.Eax = 0x4800;
        Registers.Edx = i;
        Registers.Esi = (uint32_t)&Parameters;

        /* We need the bytes per sector value; We're assuming that it is valid as long as it
           isn't zero, but it might be a good idea to do a better check later.
           We also limit the sector size to 4096 bytes for now, as that is our ReadBuffer
           size. */
        memset(&Parameters, 0, sizeof(ExtDriveParameters));
        Parameters.Size = 0x1E;

        BiosCall(0x13, &Registers);
        if ((Registers.Eflags & 1) || !Parameters.BytesPerSector ||
            Parameters.BytesPerSector > 4096) {
            continue;
        }

        memcpy(&DriveParameters[i].Parameters, &Parameters, sizeof(ExtDriveParameters));
        BiProbeDisk(&DrivePartitions[i], ReadDisk, &DriveParameters[i], Parameters.BytesPerSector);
    }

    BiosBootDisk = (uint32_t)BootInfo;

    /* If the boot drive has a partition directly written on it (any "MBR" is probably invalid),
       then the boot drive is the disk, otherwise, it's the first active partition in the disk. */
    if (DriveParameters[BiosBootDisk].Base.OpenRoot) {
        BiBootPartition = (BmPartition *)&DriveParameters[BiosBootDisk];
        return;
    }

    RtSList *ListHeader = DrivePartitions[BiosBootDisk].Next;
    while (ListHeader) {
        BmPartition *Partition = CONTAINING_RECORD(ListHeader, BmPartition, ListHeader);

        if (Partition->Active) {
            BiBootPartition = Partition;
            return;
        }

        ListHeader = ListHeader->Next;
    }

    /* If we got here through the MBR, we should have at least one partition on the disk,
       right??? */
    BiBootPartition = CONTAINING_RECORD(DrivePartitions[BiosBootDisk].Next, BmPartition, ListHeader);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function opens the given device.
 *     On x86, the device is either the boot partition (boot()), or a BIOS disk (bios(N)).
 *
 * PARAMETERS:
 *     Name - I/O; What device we're trying to open; This will point to either a NULL byte, or a
 *            partition identifier on success.
 *     ListHead - Output; Partition list for this disk.
 *
 * RETURN VALUE:
 *     Handle if the device exists, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
BmFile *BiOpenDevice(char **Name, RtSList **ListHead) {
    char *Start = *Name;
    char *End = NULL;
    int Drive = 0;

    if (tolower(*Start) != 'b' || tolower(*(Start + 1)) != 'i' || tolower(*(Start + 2)) != 'o' ||
        tolower(*(Start + 3)) != 's' || *(Start + 4) != '(') {
        return NULL;
    } else {
        Drive = strtol(Start + 5, &End, 16);
    }

    if (*End != ')' || !DriveParameters[Drive].Parameters.BytesPerSector) {
        return NULL;
    }

    BmFile *Handle = BmAllocateBlock(sizeof(BmFile));
    if (!Handle) {
        return NULL;
    }

    Handle->Size = 0;
    Handle->Context = &DriveParameters[Drive];
    Handle->Close = NULL;
    Handle->Read = ReadDisk;
    Handle->ReadEntry = NULL;
    Handle->Iterate = NULL;

    *Name = End + 1;
    *ListHead = &DrivePartitions[Drive];

    return Handle;
}
