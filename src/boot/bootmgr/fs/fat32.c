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
    uint32_t FileLength;
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

    FsContext->ClusterOffset = BootSector->ReservedSectors * BootSector->BytesPerSector;
    FsContext->FileCluster = BootSector->RootDirectoryCluster;
    FsContext->Directory = 1;

    memcpy(&FsContext->Parent, Context, sizeof(FileContext));
    Context->Type = FILE_TYPE_FAT32;
    Context->PrivateData = FsContext;

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
    (void)Context;
    (void)Name;
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
    (void)Context;
    (void)Buffer;
    (void)Start;
    (void)Size;

    if (Read) {
        *Read = 0;
    }

    return __STDIO_FLAGS_ERROR;
}
