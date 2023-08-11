/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <ctype.h>
#include <file.h>
#include <iso9660.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FileContext Parent;
    void *SectorBuffer;
    uint16_t BytesPerSector;
    uint64_t FileSector;
    int Directory;
} Iso9660Context;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries copying the specified ISO9660 file entry, reusing everything but the
 *     Iso9660Context.
 *
 * PARAMETERS:
 *     Context - File data to be copied.
 *     Copy - Destination of the copy.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiCopyIso9660(FileContext *Context, FileContext *Copy) {
    Iso9660Context *FsContext = Context->PrivateData;
    Iso9660Context *CopyContext = malloc(sizeof(Iso9660Context));

    if (!CopyContext) {
        return 0;
    }

    memcpy(CopyContext, FsContext, sizeof(Iso9660Context));
    Copy->Type = FILE_TYPE_ISO9660;
    Copy->PrivateData = CopyContext;
    Copy->FileLength = Context->FileLength;

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries probing an open device/partition for ISO9660.
 *
 * PARAMETERS:
 *     Context - I/O; Device where the caller is trying to probe for FSes. If the FS is ISO9660,
 *               we'll modify it to be an ISO9660 context.
 *     BytesPerSector - Size of each disk sector (for fields that use LBA values).
 *
 * RETURN VALUE:
 *     1 if the FS was ISO9660, 0 if we need to keep on searching.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeIso9660(FileContext *Context, uint16_t BytesPerSector) {
    const char ExpectedStandardIdentifier[5] = "CD001";

    char *Buffer = malloc(2048);
    Iso9660PrimaryVolumeDescriptor *VolumeDescriptor = (Iso9660PrimaryVolumeDescriptor *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* If this is a valid ISO9660 device, it should have a Primary Volume Descriptor (PVD) we
       can sanity check. The volume descriptors all start at the 32KiB mark (independent of
       sector size).
       The implementation here (and on the boot sector) is based of the specification available at
       https://wiki.osdev.org/ISO_9660. */

    uint64_t Offset = 0x8000;
    while (1) {
        if (__fread(Context, Offset, Buffer, 2048, NULL) || VolumeDescriptor->TypeCode == 255 ||
            memcmp(VolumeDescriptor->StandardIdentifier, ExpectedStandardIdentifier, 5) ||
            VolumeDescriptor->Version != 1) {
            free(Buffer);
            return 0;
        } else if (VolumeDescriptor->TypeCode == 1) {
            break;
        }

        Offset += 2048;
    }

    if (VolumeDescriptor->FileStructureVersion != 1) {
        free(Buffer);
        return 0;
    }

    /* The root directory should be located inside the PVD; As such, we should have nothing left
       to do. */
    Iso9660Context *FsContext = malloc(sizeof(Iso9660Context));
    if (!FsContext) {
        free(Buffer);
        return 0;
    }

    FsContext->BytesPerSector = BytesPerSector;
    FsContext->FileSector = VolumeDescriptor->RootDirectory.ExtentSector;
    FsContext->Directory = 1;

    if (BytesPerSector == 2048) {
        FsContext->SectorBuffer = Buffer;
    } else {
        FsContext->SectorBuffer = malloc(BytesPerSector);
        if (!FsContext->SectorBuffer) {
            free(FsContext);
            free(Buffer);
            return 0;
        }
    }

    memcpy(&FsContext->Parent, Context, sizeof(FileContext));
    Context->Type = FILE_TYPE_ISO9660;
    Context->PrivateData = FsContext;
    Context->FileLength = VolumeDescriptor->RootDirectory.ExtentSize;

    if (FsContext->SectorBuffer != Buffer) {
        free(Buffer);
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the cleanup of an open ISO9660 directory or file, and free()s the
 *     Context pointer.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiCleanupIso9660(FileContext *Context) {
    free(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a ISO9660 directory (if its a file, we error
 *     out), searching for a specific node.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiTraverseIso9660Directory(FileContext *Context, const char *Name) {
    Iso9660Context *FsContext = Context->PrivateData;
    uint64_t Remaining = Context->FileLength;
    uint64_t Sector = FsContext->FileSector;
    char *Current = NULL;

    if (!FsContext->Directory) {
        return 0;
    }

    int HasDot = strchr(Name, '.') != NULL;

    /* We're not sure of the exact size of the disk, so we'll be searching sector-by-sector. */
    while (Remaining) {
        if (!Current || (void *)Current - FsContext->SectorBuffer >= FsContext->BytesPerSector) {
            if (__fread(
                    &FsContext->Parent,
                    Sector * FsContext->BytesPerSector,
                    FsContext->SectorBuffer,
                    FsContext->BytesPerSector,
                    NULL)) {
                return 0;
            }

            Current = FsContext->SectorBuffer;
            Sector++;
        }

        Iso9660DirectoryRecord *Record = (Iso9660DirectoryRecord *)Current;
        char *ThisNamePos = Current + sizeof(Iso9660DirectoryRecord);
        const char *SearchNamePos = Name;
        int Match = 1;

        /* We currently can't handle level 3 multi-extent (over 4GiB) files. */
        if ((Record->FileFlags & 0x80) || !Record->DirectoryRecordLength) {
            return 0;
        }

        /* The filename to search for on the FS depends on if we're a directory, and if we have a
           dot within the filename; Other than that, we handle both searching fully on spec and bit
           out of spec (no ;1 on the end). */
        while (*SearchNamePos && Record->NameLength) {
            if (tolower(*ThisNamePos) != tolower(*SearchNamePos)) {
                Match = 0;
                break;
            }

            ThisNamePos++;
            SearchNamePos++;
            Record->NameLength--;
        }

        if ((Match && !Record->NameLength) ||
            (((Record->FileFlags & 0x02) || HasDot) && Record->NameLength == 2 &&
             *ThisNamePos == ';' && *(ThisNamePos + 1) == '1') ||
            ((!(Record->FileFlags & 0x02) && !HasDot) && Record->NameLength == 3 &&
             *ThisNamePos == '.' && *(ThisNamePos + 1) == ';' && *(ThisNamePos + 2) == '1')) {
            FsContext->FileSector = Record->ExtentSector;
            FsContext->Directory = Record->FileFlags & 0x02;
            Context->FileLength = Record->ExtentSize;
            return 1;
        }

        Current += Record->DirectoryRecordLength;
        Remaining -= Record->DirectoryRecordLength;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be an ISO9660 file. Calling it on a directory will
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
int BiReadIso9660File(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read) {
    Iso9660Context *FsContext = Context->PrivateData;
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

    /* Files in ISO9660 are on sequential sectors, so we can just __fread the parent device. */
    Flags |= __fread(
        &FsContext->Parent,
        FsContext->FileSector * FsContext->BytesPerSector + Start,
        Buffer,
        Size,
        Read);

    return Flags;
}
