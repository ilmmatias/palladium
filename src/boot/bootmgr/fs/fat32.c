/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <fat32.h>
#include <file.h>
#include <memory.h>
#include <string.h>

typedef struct {
    BmPartition *Partition;
    BmFileReadFn ReadPartition;
    void *ClusterBuffer;
    uint16_t BytesPerCluster;
    uint32_t FatOffset;
    uint32_t ClusterOffset;
    uint32_t RootCluster;
} Fat32FsContext;

typedef struct {
    Fat32FsContext *FsContext;
    uint32_t FileCluster;
    uint64_t Size;
    int Directory;
} Fat32Context;

static char *Iterate(void *ContextPtr, int Index);

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
static int FollowCluster(Fat32FsContext *FsContext, void **Current, uint32_t *Cluster) {
    ptrdiff_t Offset = Current ? *Current - FsContext->ClusterBuffer : 0;

    if (Offset && Offset < FsContext->BytesPerCluster) {
        return 1;
    } else if (Offset || !Current) {
        if (!FsContext->ReadPartition(
                FsContext->Partition,
                FsContext->FatOffset + (*Cluster << 2),
                4,
                FsContext->ClusterBuffer)) {
            return 0;
        }

        *Cluster = *((uint32_t *)FsContext->ClusterBuffer) & 0xFFFFFFF;

        if (*Cluster > 0xFFFFFF6) {
            return 0;
        }
    }

    if (Current) {
        *Current = FsContext->ClusterBuffer;
        return FsContext->ReadPartition(
            FsContext->Partition,
            FsContext->ClusterOffset + (*Cluster - 2) * FsContext->BytesPerCluster,
            FsContext->BytesPerCluster,
            FsContext->ClusterBuffer);
    } else {
        return 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a given character can exist inside a short DOS name. We do not
 *     handle dots, those need some special handling!
 *
 * PARAMETERS:
 *     Character - Which character to check.
 *
 * RETURN VALUE:
 *     1 if its a valid DOS name character, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int CheckShortNameCharacter(char Character) {
    return isupper(Character) || isdigit(Character) || (uint8_t)Character >= 128 ||
           Character == '!' || Character == '#' || Character == '$' || Character == '%' ||
           Character == '&' || Character == '\'' || Character == '(' || Character == ')' ||
           Character == '-' || Character == '@' || Character == '^' || Character == '_' ||
           Character == '`' || Character == '{' || Character == '}' || Character == '~';
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts a long file name into a short DOS equivalent.
 *
 * PARAMETERS:
 *     Name - String which we'll be converting.
 *     ShortName - Output; Where we should store the result.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ConvertIntoShortName(const char *Name, char ShortName[11]) {
    size_t Accum = 0;

    /* Leading periods are not allowed. */
    while (*Name == '.') {
        Name++;
    }

    while (*Name && *Name != '.' && Accum < 8) {
        /* Spaces are allowed (in the base name), but we need to make sure we handle trailing
           spaces (they get ignore, instead of truncated). */
        if (*Name == ' ') {
            while (*Name == ' ' && Accum < 8) {
                *(ShortName++) = *(Name++);
                Accum++;
            }

            while (*Name == ' ') {
                Name++;
            }
        } else if (CheckShortNameCharacter(*Name)) {
            *(ShortName++) = *(Name++);
            Accum++;
        } else if (islower(*Name)) {
            *(ShortName++) = toupper(*(Name++));
            Accum++;
        } else {
            *(ShortName++) = '_';
            Accum++;
            Name++;
        }
    }

    /* If we reached the limit and there is no dot, we need to truncate into 6 characters, and
       append `~1`. */
    if (*Name != '.' && *Name) {
        *(ShortName - 2) = '~';
        *(ShortName - 1) = '1';
    }

    while (*Name != '.' && *Name) {
        Name++;
    }

    if (!*Name) {
        while (Accum < 11) {
            *(ShortName++) = ' ';
            Accum++;
        }

        return;
    }

    /* On the other hand, if we reached the dot but we still don't have 8 characters, pad with
       spaces. */
    while (Accum < 8) {
        *(ShortName++) = ' ';
        Accum++;
    }

    if (*Name == '.') {
        Name++;
    }

    /* File extensions are just truncated, no special handling (just make sure we stop at
       11 characters). */
    while (*Name && Accum < 11) {
        if (CheckShortNameCharacter(*Name)) {
            *(ShortName++) = *(Name++);
            Accum++;
        } else if (islower(*Name)) {
            *(ShortName++) = toupper(*(Name++));
            Accum++;
        } else {
            *(ShortName++) = '_';
            Accum++;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be a FAT32 file. Calling it on a directory will
 *     fail.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to Fat32Context struct.
 *     Offset - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int Read(void *ContextPtr, uint64_t Offset, uint64_t Size, void *Buffer) {
    Fat32Context *Context = ContextPtr;
    Fat32FsContext *FsContext = Context->FsContext;
    uint32_t Cluster = Context->FileCluster;
    char *Current = FsContext->ClusterBuffer;
    char *Output = Buffer;

    if (Context->Directory || Offset + Size > Context->Size) {
        return 0;
    }

    /* Seek through the file into the starting cluster (for the given byte index). */
    while (Offset >= FsContext->BytesPerCluster) {
        Offset -= FsContext->BytesPerCluster;
        if (!FollowCluster(FsContext, NULL, &Cluster)) {
            return 0;
        }
    }

    while (Size) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster)) {
            return 0;
        }

        uint64_t CopySize = FsContext->BytesPerCluster - Offset;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(Output, Current + Offset, CopySize);
        Current += FsContext->BytesPerCluster;
        Output += CopySize;
        Offset = 0;
        Size -= CopySize;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a FAT32 directory (if its a file, we error
 *     out), searching for a specific named node.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to Fat32Context struct.
 *     Name - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *ReadEntry(void *ContextPtr, const char *Name) {
    Fat32Context *Context = ContextPtr;
    Fat32FsContext *FsContext = Context->FsContext;
    Fat32DirectoryEntry *Current = FsContext->ClusterBuffer;
    uint32_t Cluster = Context->FileCluster;
    char ShortName[11];

    if (!Context->Directory) {
        return NULL;
    }

    /* We'll use this if the entry has no LFN prefix entry. */
    ConvertIntoShortName(Name, ShortName);

    while (1) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster) || !Current->DosName[0]) {
            return NULL;
        }

        if ((uint8_t)Current->DosName[0] != 0xE5 && !memcmp(Current->DosName, ShortName, 11)) {
            BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(Fat32Context));
            if (!Handle) {
                return NULL;
            }

            Context = (Fat32Context *)(Handle + 1);
            Context->FsContext = FsContext;
            Context->FileCluster =
                ((uint32_t)Current->FileClusterHigh << 16) | Current->FileClusterLow;
            Context->Size = Current->FileSize;
            Context->Directory = Current->Attributes & 0x10;

            Handle->Size = Current->FileSize;
            Handle->Context = Context;
            Handle->Close = NULL;
            Handle->Read = Read;
            Handle->ReadEntry = ReadEntry;
            Handle->Iterate = Iterate;

            return Handle;
        }

        Current++;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the name of the Nth entry on what should be a FAT32 directory (if its a
 *     file, we error out).
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to Fat32Context struct.
 *     Index - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File name on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static char *Iterate(void *ContextPtr, int Index) {
    Fat32Context *Context = ContextPtr;
    Fat32FsContext *FsContext = Context->FsContext;
    Fat32DirectoryEntry *Current = FsContext->ClusterBuffer;
    uint32_t Cluster = Context->FileCluster;

    if (!Context->Directory) {
        return NULL;
    }

    for (int CurrentIndex = 0;; Current++) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster) || !Current->DosName[0]) {
            return NULL;
        } else if (CurrentIndex > Index) {
            return NULL;
        }

        if ((uint8_t)Current->DosName[0] == 0xE5 || Current->Attributes == 0x08 ||
            Current->Attributes == 0x0F || !memcmp(Current->DosName, ".          ", 11) ||
            !memcmp(Current->DosName, "..         ", 11)) {
            continue;
        }

        if (CurrentIndex != Index) {
            CurrentIndex++;
            continue;
        }

        char *Name = BmAllocateZeroBlock(13, 1);
        if (!Name) {
            return NULL;
        }

        memcpy(Name, Current->DosName, 11);

        /* Names without an extension stay the same; 8.3 names get a dot added before the
           extension. */
        char *Start = strchr(Name, ' ');
        char *Current = Start + 1;
        char *End = Name + 12;
        while (*Current && Current < End) {
            if (*Current != ' ') {
                *Start = '.';
                memcpy(Start + 1, Current, End - Current);
                break;
            }

            Current++;
        }

        return Name;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a root directory file handle.
 *
 * PARAMETERS:
 *     ContextPtr - FAT32-specific partition data.
 *
 * RETURN VALUE:
 *     New handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *OpenRoot(void *ContextPtr) {
    Fat32FsContext *FsContext = ContextPtr;
    BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(Fat32Context));
    if (!Handle) {
        return NULL;
    }

    Fat32Context *Context = (Fat32Context *)(Handle + 1);
    Context->FsContext = FsContext;
    Context->FileCluster = FsContext->RootCluster;
    Context->Size = 0;
    Context->Directory = 1;

    Handle->Size = 0;
    Handle->Context = Context;
    Handle->Close = NULL;
    Handle->Read = Read;
    Handle->ReadEntry = ReadEntry;
    Handle->Iterate = Iterate;

    return Handle;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function probes the given partition for the FAT32 filesystem.
 *
 * PARAMETERS:
 *     Partition - Which partition we're checking.
 *     ReadPartition - Function to read some bytes from the partition.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeFat32(BmPartition *Partition, BmFileReadFn ReadPartition) {
    const char ExpectedSystemIdentifier[8] = "FAT32   ";

    char *Buffer = BmAllocateBlock(512);
    Fat32BootSector *BootSector = (Fat32BootSector *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* We're using the process from http://jdebp.info/FGA/determining-filesystem-type.html to
       determine if this is really FAT32 (or if it at least looks like it). */
    if (!ReadPartition(Partition, 0, 512, Buffer) ||
        memcmp(BootSector->SystemIdentifier, ExpectedSystemIdentifier, 8) ||
        !(BootSector->BytesPerSector >= 256 && BootSector->BytesPerSector <= 4096) ||
        !BootSector->SectorsPerCluster || BootSector->SectorsPerCluster > 128 ||
        (BootSector->Signature != 0x28 && BootSector->Signature != 0x29)) {
        return 0;
    }

    Fat32FsContext *FsContext = BmAllocateBlock(sizeof(Fat32FsContext));
    if (!FsContext) {
        BmFreeBlock(Buffer);
        return 0;
    }

    FsContext->Partition = Partition;
    FsContext->ReadPartition = ReadPartition;
    FsContext->BytesPerCluster = BootSector->BytesPerSector * BootSector->SectorsPerCluster;
    FsContext->ClusterBuffer = BmAllocateBlock(FsContext->BytesPerCluster);
    if (!FsContext->ClusterBuffer) {
        BmFreeBlock(FsContext);
        BmFreeBlock(Buffer);
        return 0;
    }

    FsContext->FatOffset = BootSector->ReservedSectors * BootSector->BytesPerSector;
    FsContext->ClusterOffset =
        BootSector->NumberOfFats * BootSector->SectorsPerFat * BootSector->BytesPerSector +
        FsContext->FatOffset;
    FsContext->RootCluster = BootSector->RootDirectoryCluster;

    Partition->FsContext = FsContext;
    Partition->OpenRoot = OpenRoot;

    BmFreeBlock(Buffer);
    return 1;
}
