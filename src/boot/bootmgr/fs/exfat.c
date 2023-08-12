/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <ctype.h>
#include <exfat.h>
#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FileContext Parent;
    void *ClusterBuffer;
    uint8_t ClusterShift;
    uint8_t SectorShift;
    uint64_t ClusterOffset;
    uint64_t FatOffset;
    uint32_t FileCluster;
    int NoFatChain;
    int Directory;
} ExfatContext;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries copying the specified exFAT file entry, reusing everything but the
 *     ExfatContext.
 *
 * PARAMETERS:
 *     Context - File data to be copied.
 *     Copy - Destination of the copy.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiCopyExfat(FileContext *Context, FileContext *Copy) {
    ExfatContext *FsContext = Context->PrivateData;
    ExfatContext *CopyContext = malloc(sizeof(ExfatContext));

    if (!CopyContext) {
        return 0;
    }

    memcpy(CopyContext, FsContext, sizeof(ExfatContext));
    Copy->Type = FILE_TYPE_EXFAT;
    Copy->PrivateData = CopyContext;
    Copy->FileLength = Context->FileLength;

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries probing an open device/partition for exFAT.
 *
 * PARAMETERS:
 *     Context - I/O; Device where the caller is trying to probe for FSes. If the FS is exFAT,
 *               we'll modify it to be an exFAT context.
 *
 * RETURN VALUE:
 *     1 if the FS was exFAT, 0 if we need to keep on searching.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeExfat(FileContext *Context) {
    const char ExpectedJumpBoot[3] = {0xEB, 0x76, 0x90};
    const char ExpectedFileSystemName[8] = "EXFAT   ";

    char *Buffer = malloc(512);
    ExfatBootSector *BootSector = (ExfatBootSector *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* We can use the mandatory fields described on the Main Boot Sector structure at
       https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification to quickly do
       the probing. */

    if (__fread(Context, 0, Buffer, 512, NULL) ||
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

    FsContext->ClusterShift = BootSector->BytesPerSectorShift + BootSector->SectorsPerClusterShift;
    FsContext->ClusterBuffer = malloc(1 << FsContext->ClusterShift);
    if (!FsContext->ClusterBuffer) {
        free(FsContext);
        free(Buffer);
        return 0;
    }

    FsContext->SectorShift = BootSector->BytesPerSectorShift;
    FsContext->ClusterOffset = BootSector->ClusterHeapOffset << BootSector->BytesPerSectorShift;
    FsContext->FatOffset = BootSector->FatOffset << BootSector->BytesPerSectorShift;
    FsContext->FileCluster = BootSector->FirstClusterOfRootDirectory - 2;
    FsContext->Directory = 1;

    /* Grab the NoFatChain flag from the root directory (the first entry of a directory
       tells us that info). */
    if (__fread(
            Context,
            FsContext->ClusterOffset + (FsContext->FileCluster << FsContext->ClusterShift),
            FsContext->ClusterBuffer,
            sizeof(ExfatDirectoryEntry),
            NULL)) {
        free(FsContext);
        free(Buffer);
        return 0;
    }

    FsContext->NoFatChain =
        ((ExfatDirectoryEntry *)FsContext->ClusterBuffer)->FileAttributes & 0x02;

    memcpy(&FsContext->Parent, Context, sizeof(FileContext));
    Context->Type = FILE_TYPE_EXFAT;
    Context->PrivateData = FsContext;
    Context->FileLength = 0;

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
void BiCleanupExfat(FileContext *Context) {
    free(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if we need to read in a cluster, following the FAT if required.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     Current - I/O; Current position along the cluster buffer.
 *     Cluster - I/O; Current cluster (minus 2, as usual on FAT32/exFAT).
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
static int FollowCluster(ExfatContext *FsContext, void **Current, uint64_t *Cluster) {
    ptrdiff_t Offset = Current ? *Current - FsContext->ClusterBuffer : 0;

    if (Offset && Offset < (1 << FsContext->ClusterShift)) {
        return 1;
    } else if (Offset || !Current) {
        if (FsContext->NoFatChain) {
            (*Cluster)++;
        } else if (__fread(
                       &FsContext->Parent,
                       FsContext->FatOffset + (*Cluster << 2),
                       FsContext->ClusterBuffer,
                       4,
                       NULL)) {
            return 0;
        } else {
            *Cluster = *((uint32_t *)FsContext->ClusterBuffer);
            if (*Cluster > 0xFFFFFFF6) {
                return 0;
            }
        }
    }

    if (Current) {
        *Current = FsContext->ClusterBuffer;
        return !__fread(
            &FsContext->Parent,
            FsContext->ClusterOffset + (*Cluster << FsContext->ClusterShift),
            FsContext->ClusterBuffer,
            1 << FsContext->ClusterShift,
            NULL);
    } else {
        return 1;
    }
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
int BiTraverseExfatDirectory(FileContext *Context, const char *Name) {
    ExfatContext *FsContext = Context->PrivateData;
    ExfatDirectoryEntry *Current = FsContext->ClusterBuffer;
    uint64_t Cluster = FsContext->FileCluster;
    size_t NameLength = strlen(Name);

    if (!FsContext->Directory) {
        return 0;
    }

    while (1) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster) || !Current->EntryType) {
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

        if (!FollowCluster(FsContext, (void **)&Current, &Cluster)) {
            return 0;
        } else if (
            Current->EntryType != 0xC0 || ((ExfatStreamEntry *)Current)->NameLength != NameLength) {
            while (Entry.SecondaryCount--) {
                Current++;
                if (!FollowCluster(FsContext, (void **)&Current, &Cluster)) {
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
            if (!FollowCluster(FsContext, (void **)&Current, &Cluster)) {
                return 0;
            } else if (Current->EntryType != 0xC1) {
                Entry.SecondaryCount--;
                break;
            }

            ExfatFileNameEntry *NameEntry = (ExfatFileNameEntry *)Current;

            for (int i = 0; i < (StreamEntry.NameLength < 15 ? StreamEntry.NameLength : 15); i++) {
                if (tolower(NameEntry->FileName[i]) != tolower(*(NamePos++))) {
                    Match = 0;
                    break;
                }
            }

            Current++;
            Entry.SecondaryCount--;
            StreamEntry.NameLength = StreamEntry.NameLength > 15 ? StreamEntry.NameLength - 15 : 0;
        }

        if (!Match) {
            while (Entry.SecondaryCount--) {
                Current++;
                if (!FollowCluster(FsContext, (void **)&Current, &Cluster)) {
                    return 0;
                }
            }

            continue;
        }

        /* Name match, setup the file entry to reflect the new file/directory we opened, and
           we're done. */

        FsContext->FileCluster = StreamEntry.FirstCluster - 2;
        FsContext->NoFatChain = StreamEntry.GeneralSecondaryFlags & 0x02;
        FsContext->Directory = Entry.FileAttributes & 0x10;
        Context->FileLength = StreamEntry.ValidDataLength;

        return 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be an exFAT file. Calling it on a directory will
 *     fail.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Buffer - Output buffer.
 *     Start - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Read - How many bytes we read with no error.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiReadExfatFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read) {
    ExfatContext *FsContext = Context->PrivateData;
    uint64_t Cluster = FsContext->FileCluster;
    char *Current = FsContext->ClusterBuffer;
    char *Output = Buffer;
    size_t Accum = 0;
    int Flags = 0;

    if (FsContext->Directory) {
        return __STDIO_FLAGS_ERROR;
    } else if (Start > Context->FileLength) {
        return __STDIO_FLAGS_EOF;
    }

    if (Size > Context->FileLength - Start) {
        Flags = __STDIO_FLAGS_EOF;
        Size = Context->FileLength - Start;
    }

    /* Seek through the file into the starting cluster (for the given byte index). */
    while (Start >= (1 << FsContext->ClusterShift)) {
        Start -= 1 << FsContext->ClusterShift;
        if (!FollowCluster(FsContext, NULL, &Cluster)) {
            if (Read) {
                *Read = 0;
            }

            return __STDIO_FLAGS_ERROR;
        }
    }

    while (Size) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster)) {
            if (Read) {
                *Read = Accum;
            }

            return __STDIO_FLAGS_ERROR;
        }

        size_t CopySize = (1 << FsContext->ClusterShift) - Start;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(Output, Current + Start, CopySize);
        Current += 1 << FsContext->ClusterShift;
        Output += CopySize;
        Start += CopySize;
        Size -= CopySize;
        Accum += CopySize;
    }

    if (Read) {
        *Read = Accum;
    }

    return Flags;
}
