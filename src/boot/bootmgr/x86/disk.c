/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <crt_impl.h>
#include <device.h>
#include <stdalign.h>
#include <stdio.h>
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
static uint8_t BootDrive;

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
void BiosDetectDisks(BiosBootBlock *Data) {
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
    BootDrive = Data->BootDrive;

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
 *     This function parses the given path segment, opening the respective disk.
 *
 * PARAMETERS:
 *     Segment - Path segment string.
 *     Context - Output; Set to the private data required to manipulate the disk.
 *
 * RETURN VALUE:
 *     How many characters we consumed if the disk was valid, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiOpenArchDevice(const char *Segment, DeviceContext *Context) {
    /* `bios(n)part(n)/<file> (ARC-like). This function parses the first part (`bios(n)`).
       You can also pass `bios()` instead of `bios(n)`, indicating that we should open the boot
       device. */
    if ((*Segment != 'b' && *Segment != 'B') || (*(Segment + 1) != 'i' && *(Segment + 1) != 'I') ||
        (*(Segment + 2) != 'o' && *(Segment + 2) != 'O') ||
        (*(Segment + 3) != 's' && *(Segment + 3) != 'S') || (*(Segment + 4) != '(')) {
        return 0;
    }

    char *End;
    int Drive = strtol(Segment + 5, &End, 16);

    Context->Type = DEVICE_TYPE_ARCH;

    if (Segment + 5 == End) {
        Context->PrivateData = (void *)(uint32_t)BootDrive;
    } else if (Drive > 255 || !DriveParameters[Drive].Size) {
        return 0;
    } else {
        Context->PrivateData = (void *)(uint32_t)Drive;
    }

    return *End == ')' ? End - Segment + 1 : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the cleanup of an open arch-specific device, and free()s the Context
 *     pointer.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiFreeArchDevice(DeviceContext *Context) {
    free(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function uses the BIOS to read sectors from one of the detected disks.
 *
 * PARAMETERS:
 *     Context - Expected to be a valid arch device context.
 *     Buffer - Output buffer.
 *     Start - Starting byte index (in the disk).
 *     Size - How many bytes to read into the buffer.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiReadArchDevice(DeviceContext *Context, void *Buffer, uint64_t Start, size_t Size) {
    uint8_t Drive = (uint8_t)(uint32_t)Context->PrivateData;
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
            return __STDIO_FLAGS_ERROR;
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

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function would travesal the arch-specific device, but as this should be a raw disk,
 *     there's no traversal to be done.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0; Always 0 here.
 *-----------------------------------------------------------------------------------------------*/
int BiReadArchDirectoryEntry(DeviceContext *Context, const char *Name) {
    (void)Context;
    (void)Name;
    return 0;
}
