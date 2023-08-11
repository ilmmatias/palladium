/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <ctype.h>
#include <fat32.h>
#include <file.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FileContext Parent;
    void *ClusterBuffer;
    uint16_t BytesPerCluster;
    uint32_t ClusterOffset;
    uint32_t FileCluster;
    int Directory;
} Fat32Context;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries copying the specified FAT32 file entry, reusing everything but the
 *     Fat32Context.
 *
 * PARAMETERS:
 *     Context - File data to be copied.
 *     Copy - Destination of the copy.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiCopyFat32(FileContext *Context, FileContext *Copy) {
    Fat32Context *FsContext = Context->PrivateData;
    Fat32Context *CopyContext = malloc(sizeof(Fat32Context));

    if (!CopyContext) {
        return 0;
    }

    memcpy(CopyContext, FsContext, sizeof(Fat32Context));
    Copy->Type = FILE_TYPE_FAT32;
    Copy->PrivateData = CopyContext;
    Copy->FileLength = Context->FileLength;

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries probing an open device/partition for FAT32.
 *
 * PARAMETERS:
 *     Context - I/O; Device where the caller is trying to probe for FSes. If the FS is FAT32,
 *               we'll modify it to be an FAT32 context.
 *
 * RETURN VALUE:
 *     1 if the FS was FAT32, 0 if we need to keep on searching.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeFat32(FileContext *Context) {
    const char ExpectedSystemIdentifier[8] = "FAT32   ";

    char *Buffer = malloc(512);
    Fat32BootSector *BootSector = (Fat32BootSector *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* We're using the process from http://jdebp.info/FGA/determining-filesystem-type.html to
       determine if this is really FAT32 (or if it at least looks like it). */
    if (__fread(Context, 0, Buffer, 512, NULL) ||
        memcmp(BootSector->SystemIdentifier, ExpectedSystemIdentifier, 8) ||
        !(BootSector->BytesPerSector >= 256 && BootSector->BytesPerSector <= 4096) ||
        !BootSector->SectorsPerCluster || BootSector->SectorsPerCluster > 128 ||
        (BootSector->Signature != 0x28 && BootSector->Signature != 0x29)) {
        return 0;
    }

    Fat32Context *FsContext = malloc(sizeof(Fat32Context));
    if (!FsContext) {
        free(Buffer);
        return 0;
    }

    FsContext->BytesPerCluster = BootSector->BytesPerSector * BootSector->SectorsPerCluster;
    FsContext->ClusterBuffer = malloc(FsContext->BytesPerCluster);
    if (!FsContext->ClusterBuffer) {
        free(FsContext);
        free(Buffer);
        return 0;
    }

    FsContext->ClusterOffset =
        (BootSector->NumberOfFats * BootSector->SectorsPerFat + BootSector->ReservedSectors) *
        BootSector->BytesPerSector;
    FsContext->FileCluster = BootSector->RootDirectoryCluster - 2;
    FsContext->Directory = 1;

    memcpy(&FsContext->Parent, Context, sizeof(FileContext));
    Context->Type = FILE_TYPE_FAT32;
    Context->PrivateData = FsContext;
    Context->FileLength = 0;

    free(Buffer);
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the cleanup of an open FAT32 directory or file, and free()s the
 *     Context pointer.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiCleanupFat32(FileContext *Context) {
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
static int FollowCluster(Fat32Context *FsContext, void **Current, uint32_t *Cluster) {
    ptrdiff_t Offset = Current ? *Current - FsContext->ClusterBuffer : 0;

    if (Offset && Offset < FsContext->BytesPerCluster) {
        return 1;
    } else if (Offset || !Current) {
        if (__fread(
                &FsContext->Parent,
                FsContext->ClusterOffset + (*Cluster << 2),
                FsContext->ClusterBuffer,
                4,
                NULL)) {
            return 0;
        }

        *Cluster = *((uint32_t *)FsContext->ClusterBuffer) & 0xFFFFFFF;

        if (*Cluster > 0xFFFFFF6) {
            return 0;
        }
    }

    if (Current) {
        *Current = FsContext->ClusterBuffer;
        return !__fread(
            &FsContext->Parent,
            FsContext->ClusterOffset + *Cluster * FsContext->BytesPerCluster,
            FsContext->ClusterBuffer,
            FsContext->BytesPerCluster,
            NULL);
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
 *     This function does traversal on what should be a FAT32 directory (if its a file, we error
 *     out), searching for a specific node.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiTraverseFat32Directory(FileContext *Context, const char *Name) {
    Fat32Context *FsContext = Context->PrivateData;
    Fat32DirectoryEntry *Current = FsContext->ClusterBuffer;
    uint32_t Cluster = FsContext->FileCluster;
    char ShortName[11];

    if (!FsContext->Directory) {
        return 0;
    }

    /* We'll use this if the entry has no LFN prefix entry. */
    ConvertIntoShortName(Name, ShortName);

    while (1) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster) || !Current->DosName[0]) {
            return 0;
        } else if (
            (uint8_t)Current->DosName[0] != 0xE5 && !memcmp(Current->DosName, ShortName, 11)) {
            FsContext->FileCluster =
                (((uint32_t)Current->FileClusterHigh << 16) | Current->FileClusterLow) - 2;
            FsContext->Directory = Current->Attributes & 0x10;
            Context->FileLength = Current->FileSize;
            return 1;
        }

        Current++;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be an FAT32 file. Calling it on a directory will
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
int BiReadFat32File(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read) {
    Fat32Context *FsContext = Context->PrivateData;
    uint32_t Cluster = FsContext->FileCluster;
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
    while (Start >= FsContext->BytesPerCluster) {
        Start -= FsContext->BytesPerCluster;
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

        size_t CopySize = FsContext->BytesPerCluster - Start;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(Output, Current + Start, CopySize);
        Current += FsContext->BytesPerCluster;
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
