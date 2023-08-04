/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ctype.h>
#include <device.h>
#include <exfat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    DeviceContext Parent;
    void *SectorBuffer;
    uint8_t ClusterShift;
    uint8_t SectorShift;
    uint64_t ClusterOffset;
    uint32_t FileCluster;
    int Directory;
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
    FsContext->ClusterShift = BootSector->BytesPerSectorShift + BootSector->SectorsPerClusterShift;
    FsContext->SectorShift = BootSector->BytesPerSectorShift;
    FsContext->ClusterOffset = BootSector->ClusterHeapOffset << BootSector->BytesPerSectorShift;
    FsContext->FileCluster = BootSector->FirstClusterOfRootDirectory - 2;
    FsContext->Directory = 1;

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
    free(((ExfatContext *)Context)->SectorBuffer);
    free(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates that the current sector is inside the cluster, and reloads if we
 *     crossed over a sector boundary (but not a cluster boundary).
 *
 * PARAMETERS:
 *     FsContext - FS and current directory entry data.
 *     Current - I/O; Current position of the sector buffer.
 *     Sector - Start of the current cluster.
 *     SectorOffset - I/O; Current offset from the start of the cluster.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
static int ValidateSector(
    ExfatContext *FsContext,
    ExfatDirectoryEntry **Current,
    uint64_t Sector,
    uint64_t *SectorOffset) {
    size_t BufferOffset = (size_t)((char *)*Current - (char *)FsContext->SectorBuffer);

    if (BufferOffset && BufferOffset < (1 << FsContext->SectorShift)) {
        return 1;
    } else if (*SectorOffset >= (1 << FsContext->ClusterShift)) {
        return 0;
    } else if (!BiReadDevice(
                   &FsContext->Parent,
                   FsContext->SectorBuffer,
                   Sector + *SectorOffset,
                   1 << FsContext->SectorShift)) {
        return 0;
    }

    *Current = FsContext->SectorBuffer;
    *SectorOffset += 1 << FsContext->SectorShift;

    return 1;
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
    ExfatDirectoryEntry *Current = FsContext->SectorBuffer;
    uint64_t SectorOffset = 0;
    uint64_t Cluster = FsContext->FileCluster;
    uint64_t Sector = FsContext->ClusterOffset + (Cluster << FsContext->ClusterShift);

    while (1) {
        if (!ValidateSector(FsContext, &Current, Sector, &SectorOffset) || !Current->EntryType) {
            return 0;
        } else if (Current->EntryType != 0x85) {
            Current++;
            continue;
        }

        /* We might need to reload the buffer when checking the secondaries. */
        ExfatDirectoryEntry Entry;
        memcpy(&Entry, Current, sizeof(ExfatDirectoryEntry));

        /* The Stream Extension Entry is required and only valid if directly following the
           File entry we just found (section 7.4). */
        Current++;
        Entry.SecondaryCount--;

        if (!ValidateSector(FsContext, &Current, Sector, &SectorOffset)) {
            return 0;
        } else if (Current->EntryType != 0xC0) {
            while (Entry.SecondaryCount--) {
                Current++;
                if (!ValidateSector(FsContext, &Current, Sector, &SectorOffset)) {
                    return 0;
                }
            }

            continue;
        }

        ExfatStreamEntry StreamEntry;
        memcpy(&StreamEntry, Current, sizeof(ExfatStreamEntry));

        /* File Name entries should only be valid directly following the stream entry. */
        const char *NamePos = Name;
        int Match = 1;
        Current++;

        while (StreamEntry.NameLength && Match) {
            if (!ValidateSector(FsContext, &Current, Sector, &SectorOffset)) {
                return 0;
            } else if (Current->EntryType != 0xC1) {
                Entry.SecondaryCount--;
                break;
            }

            ExfatFileNameEntry *NameEntry = (ExfatFileNameEntry *)Current;

            for (int i = 0; i < (StreamEntry.NameLength < 15 ? StreamEntry.NameLength : 15); i++) {
                if (NameEntry->FileName[i] != tolower(*(NamePos++))) {
                    Match = 0;
                    break;
                }
            }

            Current++;
            Entry.SecondaryCount--;
            StreamEntry.NameLength = StreamEntry.NameLength > 15 ? StreamEntry.NameLength - 15 : 0;
        }

        if (!Match || *NamePos) {
            while (Entry.SecondaryCount--) {
                Current++;
                if (!ValidateSector(FsContext, &Current, Sector, &SectorOffset)) {
                    return 0;
                }
            }

            continue;
        }

        /* Name match, setup the file entry to reflect the new file/directory we opened, and
           we're done. */

        FsContext->FileCluster = StreamEntry.FirstCluster - 2;
        FsContext->Directory = Entry.FileAttributes & 0x10;

        return 1;
    }
}
