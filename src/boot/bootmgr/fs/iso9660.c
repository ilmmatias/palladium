/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ctype.h>
#include <file.h>
#include <iso9660.h>
#include <memory.h>
#include <string.h>

typedef struct {
    BmPartition *Partition;
    BmFileReadFn ReadPartition;
    void *SectorBuffer;
    uint32_t RootSector;
    uint32_t RootSize;
} Iso9660FsContext;

typedef struct {
    Iso9660FsContext *FsContext;
    uint32_t FirstSector;
    uint32_t Size;
    int Directory;
} Iso9660Context;

static char *Iterate(void *ContextPtr, int Index);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be an ISO9660 file. Calling it on a directory will
 *     fail.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to Iso9660Context struct.
 *     Offset - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int Read(void *ContextPtr, uint64_t Offset, uint64_t Size, void *Buffer) {
    Iso9660Context *Context = ContextPtr;
    Iso9660FsContext *FsContext = Context->FsContext;

    if (Context->Directory || Offset + Size > Context->Size) {
        return 0;
    }

    /* Files in ISO9660 are on sequential sectors, so we can just read the parent device. */
    return FsContext->ReadPartition(
        FsContext->Partition, (Context->FirstSector << 11) + Offset, Size, Buffer);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a ISO9660 directory (if its a file, we error
 *     out), searching for a specific named node.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to Iso9660Context struct.
 *     Name - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *ReadEntry(void *ContextPtr, const char *Name) {
    Iso9660Context *Context = ContextPtr;
    Iso9660FsContext *FsContext = Context->FsContext;
    uint32_t Remaining = Context->Size;
    uint32_t Sector = Context->FirstSector;
    char *Current = NULL;

    if (!Context->Directory) {
        return NULL;
    }

    /* We're not sure of the exact size of the disk, so we'll be searching sector-by-sector. */
    while (Remaining) {
        if (!Current || (void *)Current - FsContext->SectorBuffer >= 2048) {
            if (!FsContext->ReadPartition(
                    FsContext->Partition, Sector << 11, 2048, FsContext->SectorBuffer)) {
                return NULL;
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
            return NULL;
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

        if (!((Match && !Record->NameLength) ||
              (!*SearchNamePos &&
               ((Record->NameLength == 2 && *ThisNamePos == ';' && *(ThisNamePos + 1) == '1') ||
                (Record->NameLength == 3 && *ThisNamePos == '.' && *(ThisNamePos + 1) == ';' &&
                 *(ThisNamePos + 2) == '1'))))) {
            Current += Record->DirectoryRecordLength;
            Remaining -= Record->DirectoryRecordLength;
            continue;
        }

        BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(Iso9660Context));
        if (!Handle) {
            return NULL;
        }

        Context = (Iso9660Context *)(Handle + 1);
        Context->FsContext = FsContext;
        Context->FirstSector = Record->ExtentSector;
        Context->Size = Record->ExtentSize;
        Context->Directory = Record->FileFlags & 0x02;

        Handle->Size = Record->ExtentSize;
        Handle->Context = Context;
        Handle->Close = NULL;
        Handle->Read = Read;
        Handle->ReadEntry = ReadEntry;
        Handle->Iterate = Iterate;

        return Handle;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the name of the Nth entry on what should be a ISO9660 directory (if its
 *     a file, we error out).
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to Iso9660Context struct.
 *     Index - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File name on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static char *Iterate(void *ContextPtr, int Index) {
    Iso9660Context *Context = ContextPtr;
    Iso9660FsContext *FsContext = Context->FsContext;
    uint32_t Remaining = Context->Size;
    uint32_t Sector = Context->FirstSector;
    char *Current = NULL;

    if (!Context->Directory) {
        return NULL;
    }

    /* We're not sure of the exact size of the disk, so we'll be searching sector-by-sector. */
    int CurrentIndex = 0;
    while (Remaining) {
        if (CurrentIndex > Index) {
            return NULL;
        } else if (!Current || (void *)Current - FsContext->SectorBuffer >= 2048) {
            if (!FsContext->ReadPartition(
                    FsContext->Partition, Sector << 11, 2048, FsContext->SectorBuffer)) {
                return NULL;
            }

            Current = FsContext->SectorBuffer;
            Sector++;
        }

        Iso9660DirectoryRecord *Record = (Iso9660DirectoryRecord *)Current;
        char *NamePos = Current + sizeof(Iso9660DirectoryRecord);

        /* We currently can't handle level 3 multi-extent (over 4GiB) files. */
        if ((Record->FileFlags & 0x80) || !Record->DirectoryRecordLength) {
            return NULL;
        }

        if ((Record->NameLength == 1 && *NamePos <= 1) || CurrentIndex++ != Index) {
            Current += Record->DirectoryRecordLength;
            Remaining -= Record->DirectoryRecordLength;
            continue;
        }

        /* On fully-on-spec disks, this will probably overallocate memory by a few bytes, but it
           shouldn't be too bad. */
        char *Name = BmAllocateZeroBlock(Record->NameLength + 1, 1);
        if (!Name) {
            return NULL;
        }

        int DestPos = 0;
        while (Record->NameLength--) {
            if ((Record->NameLength == 1 && *NamePos == ';' && *(NamePos + 1) == '1') ||
                (Record->NameLength == 2 && *NamePos == '.' && *(NamePos + 1) == ';' &&
                 *(NamePos + 2) == '1')) {
                break;
            }

            Name[DestPos++] = *(NamePos++);
        }

        return Name;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a root directory file handle.
 *
 * PARAMETERS:
 *     ContextPtr - ISO9660-specific disk data.
 *
 * RETURN VALUE:
 *     New handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *OpenRoot(void *ContextPtr) {
    Iso9660FsContext *FsContext = ContextPtr;
    BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(Iso9660Context));
    if (!Handle) {
        return NULL;
    }

    Iso9660Context *Context = (Iso9660Context *)(Handle + 1);
    Context->FsContext = FsContext;
    Context->FirstSector = FsContext->RootSector;
    Context->Size = FsContext->RootSize;
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
 *     This function probes the given partition for the ISO9660 filesystem.
 *
 * PARAMETERS:
 *     Partition - Which partition we're checking.
 *     ReadPartition - Function to read some bytes from the partition.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeIso9660(BmPartition *Partition, BmFileReadFn ReadPartition) {
    const char ExpectedStandardIdentifier[5] = "CD001";

    char *Buffer = BmAllocateBlock(2048);
    Iso9660PrimaryVolumeDescriptor *VolumeDescriptor = (Iso9660PrimaryVolumeDescriptor *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* If this is a valid ISO9660 device, it should have a Primary Volume Descriptor (PVD) we
       can sanity check.
       The implementation here (and on the boot sector) is based of the specification available at
       https://wiki.osdev.org/ISO_9660. */

    uint64_t Offset = 0x8000;
    while (1) {
        if (!ReadPartition(Partition, Offset, 2048, Buffer) || VolumeDescriptor->TypeCode == 255 ||
            memcmp(VolumeDescriptor->StandardIdentifier, ExpectedStandardIdentifier, 5) ||
            VolumeDescriptor->Version != 1) {
            BmFreeBlock(Buffer);
            return 0;
        } else if (VolumeDescriptor->TypeCode == 1) {
            break;
        }

        Offset += 2048;
    }

    if (VolumeDescriptor->FileStructureVersion != 1) {
        BmFreeBlock(Buffer);
        return 0;
    }

    /* The root directory should be located inside the PVD; As such, we should have nothing left
       to do. */
    Iso9660FsContext *FsContext = BmAllocateBlock(sizeof(Iso9660FsContext));
    if (!FsContext) {
        BmFreeBlock(Buffer);
        return 0;
    }

    FsContext->Partition = Partition;
    FsContext->ReadPartition = ReadPartition;
    FsContext->SectorBuffer = Buffer;
    FsContext->RootSector = VolumeDescriptor->RootDirectory.ExtentSector;
    FsContext->RootSize = VolumeDescriptor->RootDirectory.ExtentSize;

    Partition->FsContext = FsContext;
    Partition->OpenRoot = OpenRoot;

    return 1;
}
