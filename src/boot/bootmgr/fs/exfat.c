/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <device.h>
#include <exfat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DeviceContext Parent;
    void *SectorBuffer;
    uint8_t BytesPerClusterShift;
    uint8_t BytesPerSectorShift;
    uint32_t ClusterHeapOffset;
    uint32_t FileCluster;
} ExfatContext;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries probing an open device/partition for exFAT.
 *
 * PARAMETERS:
 *     context - I/O; Device where the caller is trying to probe for FSes. If the FS is exFAT,
 *               we'll modify it to be an exFAT context.
 *
 * RETURN VALUE:
 *     1 if the FS was exFAT, 0 if we need to keep on searching.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeExfat(DeviceContext *Context) {
    char *Buffer = malloc(512);
    if (!Buffer) {
        return 0;
    }

    /* exFAT seems to have an actual way of validating its "BPB" + if we're really dealing with
       it, instead of having to do a bit of guessing like on FAT32 and NTFS.
       We can use the mandatory fields described on the Main Boot Sector structure at
       https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification to quickly
       probe this. */

    ExfatBootSector *BootSector = (ExfatBootSector *)Buffer;
    const char ExpectedJumpBoot[3] = {0xEB, 0x76, 0x90};
    const char ExpectedFileSystemName[8] = "EXFAT   ";

    if (!BiReadDevice(Context, Buffer, 0, 512) ||
        memcmp(BootSector->JumpBoot, ExpectedJumpBoot, 3) ||
        memcmp(BootSector->FileSystemName, ExpectedFileSystemName, 8)) {
        free(Buffer);
        return 0;
    }

    for (int i = 0; i < 53; i++) {
        if (BootSector->MustBeZero[i]) {
            free(Buffer);
            return 0;
        }
    }

    /* We can be somewhat confident now that the FS at least looks like exFAT; either the
       user wants to open the root directory (and leave at that), or open something inside it;
       either way, save the root directory to the new context struct. */
    ExfatContext *FsContext = malloc(sizeof(ExfatContext));
    if (!FsContext) {
        free(Buffer);
        return 0;
    }

    FsContext->SectorBuffer = malloc(1 << BootSector->BytesPerSectorShift);
    if (!FsContext->SectorBuffer) {
        free(FsContext);
        free(Buffer);
        return 0;
    }

    memcpy(&FsContext->Parent, Context, sizeof(DeviceContext));
    FsContext->BytesPerClusterShift =
        BootSector->BytesPerSectorShift + BootSector->SectorsPerClusterShift;
    FsContext->BytesPerSectorShift = BootSector->BytesPerSectorShift;
    FsContext->ClusterHeapOffset = BootSector->ClusterHeapOffset << BootSector->BytesPerSectorShift;
    FsContext->FileCluster = BootSector->FirstClusterOfRootDirectory - 2;

    Context->Type = DEVICE_TYPE_EXFAT;
    Context->PrivateData = FsContext;

    free(Buffer);
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the cleanup of an open exFAT directory or file, and free()s the Context
 *     pointer.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiCleanupExfat(DeviceContext *Context) {
    free(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a exFAT directory (if its a file, we error
 *     out), searching for a specific node.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiTraverseExfatDirectory(DeviceContext *Context, const char *Name) {
    ExfatContext *FsContext = Context->PrivateData;
    DeviceContext *Parent = &FsContext->Parent;
    ExfatGenericDirectoryEntry *Current = FsContext->SectorBuffer;
    ExfatGenericDirectoryEntry *Last =
        (ExfatGenericDirectoryEntry *)((char *)FsContext->SectorBuffer +
                                       (1 << FsContext->BytesPerSectorShift));
    uint32_t Sector =
        FsContext->ClusterHeapOffset + (FsContext->FileCluster << FsContext->BytesPerClusterShift);

    (void)Name;

    for (;; Current++) {
        if ((void *)Current == FsContext->SectorBuffer || Current >= Last) {
            if (!BiReadDevice(Parent, Current, Sector, 1 << FsContext->BytesPerSectorShift)) {
                return 0;
            }

            Sector += 1 << FsContext->BytesPerSectorShift;
        }

        if (!Current->EntryType) {
            return 0;
        } else if (Current->EntryType != 0x85) {
            continue;
        }
    }
}
