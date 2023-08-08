/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <file.h>
#include <ntfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FileContext Parent;
    void *ClusterBuffer;
    uint16_t BytesPerSector;
    uint32_t BytesPerCluster;
    uint32_t BytesPerMftEntry;
    uint32_t BytesPerIndexEntry;
    uint64_t MftOffset;
    uint32_t FileEntry;
    int Directory;
} NtfsContext;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries copying the specified NTFS file entry, reusing everything but the
 *     NtfsContext.
 *
 * PARAMETERS:
 *     Context - File data to be copied.
 *     Copy - Destination of the copy.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiCopyNtfs(FileContext *Context, FileContext *Copy) {
    NtfsContext *FsContext = Context->PrivateData;
    NtfsContext *CopyContext = malloc(sizeof(NtfsContext));

    if (!CopyContext) {
        return 0;
    }

    memcpy(CopyContext, FsContext, sizeof(NtfsContext));
    Copy->Type = FILE_TYPE_NTFS;
    Copy->PrivateData = CopyContext;

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries probing an open device/partition for NTFS.
 *
 * PARAMETERS:
 *     Context - I/O; Device where the caller is trying to probe for FSes. If the FS is NTFS,
 *               we'll modify it to be a NTFS context.
 *
 * RETURN VALUE:
 *     1 if the FS was NTFS, 0 if we need to keep on searching.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeNtfs(FileContext *Context) {
    const char ExpectedFileSystemName[8] = "NTFS    ";

    char *Buffer = malloc(512);
    NtfsBootSector *BootSector = (NtfsBootSector *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* Using the FileSystemName and the expected value for all fields, we should be able to
       somewhat confidently tell if this is NTFS or not.
       We're using the documentation at
       https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20(NTFS).asciidoc
     */

    if (__fread(Context, 0, Buffer, 512, NULL) ||
        memcmp(BootSector->FileSystemName, ExpectedFileSystemName, 8) ||
        !(BootSector->BytesPerSector >= 256 && BootSector->BytesPerSector <= 4096) ||
        BootSector->SectorsPerCluster > 255 || BootSector->ReservedSectors ||
        BootSector->NumberOfFats || BootSector->RootEntries || BootSector->NumberOfSectors16 ||
        BootSector->SectorsPerFat || BootSector->NumberOfSectors32 ||
        BootSector->MftEntrySize > 255 || BootSector->IndexEntrySize > 255 ||
        BootSector->SectorSignaure != 0xAA55) {
        free(Buffer);
        return 0;
    }

    /* We can be somewhat confident now that the FS at least looks like NTFS; either the
       user wants to open the root directory (and leave at that), or open something inside it;
       either way, save the root directory to the new context struct. */
    NtfsContext *FsContext = malloc(sizeof(NtfsContext));
    if (!FsContext) {
        free(Buffer);
        return 0;
    }

    FsContext->BytesPerSector = BootSector->BytesPerSector;
    FsContext->BytesPerCluster = BootSector->BytesPerSector;
    if (BootSector->SectorsPerCluster > 243) {
        FsContext->BytesPerCluster *= 1 << (256 - BootSector->SectorsPerCluster);
    } else {
        FsContext->BytesPerCluster *= BootSector->SectorsPerCluster;
    }

    FsContext->ClusterBuffer = malloc(FsContext->BytesPerCluster);
    if (!FsContext->ClusterBuffer) {
        free(FsContext);
        free(Buffer);
        return 0;
    }

    if (BootSector->MftEntrySize > 127) {
        FsContext->BytesPerMftEntry = 1 << -(int8_t)BootSector->MftEntrySize;
    } else {
        FsContext->BytesPerMftEntry = FsContext->BytesPerCluster * BootSector->MftEntrySize;
    }

    if (BootSector->IndexEntrySize > 127) {
        FsContext->BytesPerIndexEntry = 1 << -(int8_t)BootSector->IndexEntrySize;
    } else {
        FsContext->BytesPerIndexEntry = FsContext->BytesPerCluster * BootSector->IndexEntrySize;
    }

    /* MFT entry 5 is `.` (the root directory). */
    FsContext->MftOffset = BootSector->MftCluster * FsContext->BytesPerCluster;
    FsContext->FileEntry = 5;
    FsContext->Directory = 1;

    memcpy(&FsContext->Parent, Context, sizeof(FileContext));
    Context->Type = FILE_TYPE_NTFS;
    Context->PrivateData = FsContext;

    free(Buffer);
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the cleanup of an open NTFS directory or file, and free()s the Context
 *     pointer.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiCleanupNtfs(FileContext *Context) {
    free(((NtfsContext *)Context)->ClusterBuffer);
    free(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function applies/checks the fixups for any given record.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     FixupOffset - Offset from ClusterBuffer where the fixup values are located.
 *     NumberOfFixups - How many sectors to apply the fixup.
 *
 * RETURN VALUE:
 *     1 if the fixup check went OK, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ApplyFixups(NtfsContext *FsContext, uint16_t FixupOffset, uint16_t NumberOfFixups) {
    char *Buffer = (char *)FsContext->ClusterBuffer;
    uint16_t *Sector = (uint16_t *)(Buffer + FsContext->BytesPerSector - 2);
    uint16_t *FixupBuffer = (uint16_t *)(Buffer + FixupOffset);
    uint16_t Value = *FixupBuffer;

    while (NumberOfFixups-- > 1) {
        if (*Sector != Value) {
            return 0;
        }

        *Sector = *(++FixupBuffer);
        Sector += FsContext->BytesPerSector >> 1;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a given attribute inside a FILE record.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     Type - Attribute's `Type` field to search for.
 *
 * RETURN VALUE:
 *     Either a pointer to the attribute, or NULL if it couldn't be found.
 *-----------------------------------------------------------------------------------------------*/
static void *FindAttribute(NtfsContext *FsContext, uint32_t Type, int NonResident) {
    char *Start = FsContext->ClusterBuffer;
    char *Current = Start + ((NtfsMftEntry *)FsContext->ClusterBuffer)->AttributeOffset;

    while (Current - Start < (ptrdiff_t)FsContext->BytesPerCluster) {
        NtfsMftAttributeHeader *Attribute = (NtfsMftAttributeHeader *)Current;

        if (Attribute->Type == Type) {
            /* Resident mismatch means instant error; No negotiating (unless it's set to
               -1/auto). */
            if (NonResident != -1 && Attribute->NonResident != NonResident) {
                return NULL;
            } else if (Attribute->NonResident) {
                return NULL;
            } else {
                return Current + Attribute->ResidentForm.Offset;
            }
        }

        Current += Attribute->Size;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a NTFS directory (if its a file, we error
 *     out), searching for a specific node.
 *
 * PARAMETERS:
 *     Context - Device/node private data.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
int BiTraverseNtfsDirectory(FileContext *Context, const char *Name) {
    const char ExpectedSignature[4] = "FILE";

    NtfsContext *FsContext = Context->PrivateData;
    NtfsMftEntry *MftEntry = FsContext->ClusterBuffer;
    (void)Name;

    if (!FsContext->Directory ||
        __fread(
            &FsContext->Parent,
            FsContext->MftOffset + FsContext->FileEntry * FsContext->BytesPerMftEntry,
            FsContext->ClusterBuffer,
            FsContext->BytesPerCluster,
            NULL) ||
        memcmp(MftEntry->Signature, ExpectedSignature, 4) ||
        !ApplyFixups(FsContext, MftEntry->FixupOffset, MftEntry->NumberOfFixups)) {
        return 0;
    }

    /* $I30 ($INDEX_ROOT and $INDEX_ALLOCATION) are the attributes containing the directory
       tree itself (that we can iterate over). */
    NtfsIndexRootHeader *IndexRoot = FindAttribute(FsContext, 0x90, 0);
    NtfsIndexHeader *IndexHeader = (NtfsIndexHeader *)IndexRoot + 1;
    if (!IndexRoot || IndexRoot->AttributeType != 0x30 || IndexRoot->CollationType != 0x01) {
        return 0;
    }

    /* First bit SET means this is a large directory; Anything over ~16 entries cannot fit inside
       the small INDEX_ROOT. */
    if (!(IndexHeader->Flags & 1)) {
        char *Start = (char *)IndexHeader + IndexHeader->FirstEntryOffset;
        char *Current = Start;

        while (Current - Start < (ptrdiff_t)IndexHeader->TotalEntriesSize) {
            NtfsIndexEntry *IndexEntry = (NtfsIndexEntry *)Current;

            if (IndexEntry->Flags & 1) {
                break;
            }

            printf(
                "%llx, %hd, %hd, %x\n",
                IndexEntry->MftEntry,
                IndexEntry->EntryLength,
                IndexEntry->NameLength,
                IndexEntry->Flags);

            Current += IndexEntry->EntryLength;
        }

        return 0;
    }

    NtfsIndexAllocationHeader *IndexAllocation = FindAttribute(FsContext, 0xA0, 1);
    if (!IndexAllocation) {
        return 0;
    }

    return 0;
}
