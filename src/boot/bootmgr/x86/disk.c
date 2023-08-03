/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <bios.h>
#include <boot.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    uint16_t Size;
    uint16_t Flags;
    uint32_t Cylinders;
    uint32_t Heads;
    uint32_t SectorsPerTrack;
    uint64_t Sectors;
    uint16_t BytesPerSector;
    uint32_t EddPointer;
} BiosExtDriveParameters;

typedef struct __attribute__((packed)) {
    uint8_t Size;
    uint8_t AlwaysZero;
    uint16_t Sectors;
    uint16_t TransferOffset;
    uint16_t TransferSegment;
    uint64_t StartSector;
} BiosExtDrivePacket;

static char ReadBuffer[4096] __attribute__((aligned(16)));
static BiosExtDriveParameters DriveParameters[256];

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function uses the BIOS to detect all plugged in disks (that we can use extended
 *     functions).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiosDetectDisks(void) {
    BiosRegisters Registers;
    BiosExtDriveParameters Parameters;

    /* Sanity check, all our data should be below 640KB, but if it isn't its gonna break our
       BIOS read routines. */
    if (((uint32_t)ReadBuffer >> 4) > 0xFFFF) {
        BmSetColor(0x4F);
        BmInitDisplay();

        printf("An error occoured while trying to setup the boot manager environment.\n");
        printf("The disk read buffer is placed too high for BIOS usage.\n");

        while (1)
            ;
    }

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
        memset(&Parameters, 0, sizeof(BiosExtDriveParameters));
        Parameters.Size = 0x1E;

        BiosCall(0x13, &Registers);
        if ((Registers.Eflags & 1) || !Parameters.BytesPerSector ||
            Parameters.BytesPerSector > 4096) {
            continue;
        }

        memcpy(&DriveParameters[i], &Parameters, sizeof(BiosExtDriveParameters));
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function uses the BIOS to read sectors from one of the detected disks.
 *
 * PARAMETERS:
 *     Drive - BIOS drive number.
 *     Buffer - Output buffer.
 *     Start - Starting byte index (in the disk).
 *     Size - How many bytes to read into the buffer.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiosReadDisk(int Drive, void *Buffer, uint64_t Start, size_t Size) {
    if (Drive > 255 || !DriveParameters[Drive].Size) {
        return 0;
    }

    uint16_t BytesPerSector = DriveParameters[Drive].BytesPerSector;
    alignas(16) BiosExtDrivePacket Packet;
    char *OutputBuffer = Buffer;
    BiosRegisters Registers;

    /* Always read sector-by-sector, as we can't really trust the Sectors field in the DAP (
       some BIOSes don't properly implement it). */
    while (Size) {
        memset(&Registers, 0, sizeof(BiosRegisters));
        Registers.Eax = 0x4200;
        Registers.Edx = Drive;
        Registers.Esi = (uint32_t)&Packet;

        Packet.Size = 16;
        Packet.AlwaysZero = 0;
        Packet.Sectors = 1;
        Packet.TransferOffset = (uint8_t)((uint32_t)ReadBuffer & 0x0F);
        Packet.TransferSegment = (uint16_t)((uint32_t)ReadBuffer >> 4);
        Packet.StartSector = Start / BytesPerSector;

        BiosCall(0x13, &Registers);
        if (Registers.Eflags & 1) {
            return 0;
        }

        uint64_t Offset = Start % BytesPerSector;
        size_t CopySize = BytesPerSector - Offset;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(OutputBuffer, ReadBuffer + Offset, CopySize);
        OutputBuffer += CopySize;
        Start += CopySize;
        Size -= CopySize;
    }

    return 1;
}
