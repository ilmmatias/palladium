/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ctype.h>
#include <file.h>
#include <memory.h>
#include <ntfs.h>
#include <string.h>

typedef struct {
    RtSList ListHeader;
    uint64_t Vcn;
    uint64_t Lcn;
    uint64_t Length;
} NtfsDataRun;

typedef struct {
    BmPartition *Partition;
    BmFileReadFn ReadPartition;
    void *ClusterBuffer;
    uint16_t BytesPerSector;
    uint32_t BytesPerCluster;
    uint32_t BytesPerMftEntry;
    uint64_t MftOffset;
} NtfsFsContext;

typedef struct {
    NtfsFsContext *FsContext;
    RtSList DataRunListHead;
    uint64_t MftEntry;
    uint64_t Size;
    int Directory;
} NtfsContext;

static void Close(void *ContextPtr);
static int Read(void *ContextPtr, uint64_t Offset, uint64_t Size, void *Buffer);
static BmFile *ReadEntry(void *ContextPtr, const char *Name);
static char *Iterate(void *ContextPtr, int Index);

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
static int ApplyFixups(NtfsFsContext *FsContext, uint16_t FixupOffset, uint16_t NumberOfFixups) {
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
 *     This function uses a data run list obtained from FindAttribute to convert a Virtual Cluster
 *     Number (VCN) into a Logical Cluster Number (LCN, aka something we can just multiply by the
 *     size of a cluster and read it).
 *
 * PARAMETERS:
 *     DataRunListHead - List head of the saved data runs.
 *     Vcn - Virtual cluster to translate.
 *     Lcn - Output; Contains the Logical Cluster Number on success.
 *
 * RETURN VALUE:
 *     1 on match (if a translation was found), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int TranslateVcn(RtSList *DataRunListHead, uint64_t Vcn, uint64_t *Lcn) {
    RtSList *ListHeader = DataRunListHead->Next;
    while (ListHeader) {
        NtfsDataRun *Entry = CONTAINING_RECORD(ListHeader, NtfsDataRun, ListHeader);

        if (Vcn >= Entry->Vcn + Entry->Length) {
            break;
        } else if (Vcn >= Entry->Vcn) {
            *Lcn = Entry->Lcn + Vcn - Entry->Vcn;
            return 1;
        }

        ListHeader = ListHeader->Next;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a given attribute inside a FILE record.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     Type - Attribute's `Type` field to search for.
 *     NonResident - Set to either 0 or 1 depending on if the caller expects the attribute to be
 *                   resident/non-resident; Can be set to -1 for ignore/no preference.
 *     FirstVcn - Output; Starting cluster for non-resident attributes; It's a VCN, so make sure to
 *                use TranslateVcn() if you want to load it from the disk!
 *     LastVcn - Output; Ending cluster for non-resident attributes.
 *
 * RETURN VALUE:
 *     Either a pointer to the attribute, or NULL if it couldn't be found.
 *-----------------------------------------------------------------------------------------------*/
static void *FindAttribute(
    NtfsFsContext *FsContext,
    uint32_t Type,
    int NonResident,
    RtSList *DataRunListHead,
    uint64_t *FirstVcn,
    uint64_t *LastVcn) {
    char *Start = FsContext->ClusterBuffer;
    char *Current = Start + ((NtfsMftEntry *)FsContext->ClusterBuffer)->AttributeOffset;

    while (Current - Start < (ptrdiff_t)FsContext->BytesPerMftEntry) {
        NtfsMftAttributeHeader *Attribute = (NtfsMftAttributeHeader *)Current;

        if (Attribute->Type != Type) {
            Current += Attribute->Size;
            continue;
        }

        /* Resident mismatch means instant error; No negotiating (unless it's set to
           -1/auto). */
        if (NonResident != -1 && Attribute->NonResident != NonResident) {
            return NULL;
        } else if (!Attribute->NonResident) {
            if (FirstVcn) {
                *FirstVcn = UINT64_MAX;
            }

            if (LastVcn) {
                *LastVcn = UINT64_MAX;
            }

            return Current + Attribute->ResidentForm.Offset;
        }

        /* We need to load up the data runs on non-resident entries; If we already loaded up
           something for this file, we'll be assuming it is the data run list for this
           attribute. */
        if (!DataRunListHead->Next) {
            uint8_t *DataRun = (uint8_t *)Current + Attribute->NonResidentForm.DataRunOffset;
            uint64_t CurrentVcn = 0;

            while (1) {
                uint8_t Current = *(DataRun++);
                uint8_t OffsetSize = Current >> 4;
                uint8_t LengthSize = Current & 0x0F;

                if (!Current) {
                    break;
                }

                /* We shouldn't be over 8 bytes, if we are, we're just discarding those
                higher bits. */
                uint64_t Length = 0;
                memcpy((char *)&Length, DataRun, LengthSize > 8 ? 8 : LengthSize);
                DataRun += LengthSize;

                uint64_t Offset = 0;
                memcpy((char *)&Offset, DataRun, OffsetSize > 8 ? 8 : OffsetSize);
                DataRun += OffsetSize;

                /* If possible, reuse our past allocated data run list, overwriting its
                entries. */
                NtfsDataRun *Entry = BmAllocateBlock(sizeof(NtfsDataRun));
                if (!Entry) {
                    return NULL;
                }

                Entry->Vcn = CurrentVcn;
                Entry->Lcn = Offset;
                Entry->Length = Length;
                CurrentVcn += Length;

                RtPushSList(DataRunListHead, &Entry->ListHeader);
            }
        }

        if (FirstVcn) {
            *FirstVcn = Attribute->NonResidentForm.FirstVcn;
        }

        if (LastVcn) {
            *LastVcn = Attribute->NonResidentForm.LastVcn;
        }

        uint64_t Cluster;
        if (!TranslateVcn(DataRunListHead, Attribute->NonResidentForm.FirstVcn, &Cluster) ||
            !FsContext->ReadPartition(
                FsContext->Partition,
                Cluster * FsContext->BytesPerCluster,
                FsContext->BytesPerCluster,
                FsContext->ClusterBuffer)) {
            return NULL;
        }

        return FsContext->ClusterBuffer;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a given file inside an directory index block.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     HeaderStart - Where the index header starts.
 *     LastEntry - Output; Last entry of the block. Use this to check for more blocks.
 *     Name - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     Either a new BmFile with the file data, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *TraverseIndexBlockForName(
    NtfsFsContext *FsContext,
    char *HeaderStart,
    NtfsIndexRecord **LastEntry,
    const char *Name) {
    NtfsIndexHeader *IndexHeader = (NtfsIndexHeader *)HeaderStart;
    char *Start = HeaderStart + IndexHeader->FirstEntryOffset;
    char *Current = Start;
    size_t NameLength = strlen(Name);

    /* This specific block should fit inside a cluster, if it doesn't, something is wrong with
       the FS! */
    if (HeaderStart + IndexHeader->FirstEntryOffset + IndexHeader->TotalEntriesSize >
        (char *)FsContext->ClusterBuffer + FsContext->BytesPerCluster) {
        *LastEntry = NULL;
        return NULL;
    }

    while (Current - Start < (ptrdiff_t)IndexHeader->TotalEntriesSize) {
        NtfsIndexRecord *IndexEntry = (NtfsIndexRecord *)Current;
        uint16_t *ThisNamePos = (uint16_t *)(Current + sizeof(NtfsIndexRecord));
        const char *SearchNamePos = Name;
        int Match = 1;

        if (IndexEntry->Flags & 0x02) {
            break;
        } else if (IndexEntry->NameLength != NameLength) {
            Current += IndexEntry->EntryLength;
            continue;
        }

        while (*SearchNamePos) {
            if (tolower(*(ThisNamePos++)) != tolower(*(SearchNamePos++))) {
                Match = 0;
                break;
            }
        }

        if (!Match) {
            Current += IndexEntry->EntryLength;
            continue;
        }

        BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(NtfsContext));
        if (!Handle) {
            return NULL;
        }

        NtfsContext *Context = (NtfsContext *)(Handle + 1);
        Context->FsContext = FsContext;
        Context->DataRunListHead.Next = NULL;
        Context->MftEntry = IndexEntry->MftEntry & 0xFFFFFFFFFFFF;
        Context->Size = IndexEntry->RealSize;
        Context->Directory = (IndexEntry->FileFlags & 0x10000000) != 0;

        Handle->Size = IndexEntry->RealSize;
        Handle->Context = Context;
        Handle->Close = Close;
        Handle->Read = Read;
        Handle->ReadEntry = ReadEntry;
        Handle->Iterate = Iterate;

        return Handle;
    }

    *LastEntry = (NtfsIndexRecord *)Current;
    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a given index inside an directory index block.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     HeaderStart - Where the index header starts.
 *     LastEntry - Output; Last entry of the block. Use this to check for more blocks.
 *     CurrentIndex - How many entries we already checked.
 *     Index - What entry to find inside the directory.
 *
 * RETURN VALUE:
 *     Either the name of the entry, or NULL if we didn't find it here.
 *-----------------------------------------------------------------------------------------------*/
static char *TraverseIndexBlockForIndex(
    NtfsFsContext *FsContext,
    char *HeaderStart,
    NtfsIndexRecord **LastEntry,
    int *CurrentIndex,
    int Index) {
    NtfsIndexHeader *IndexHeader = (NtfsIndexHeader *)HeaderStart;
    char *Start = HeaderStart + IndexHeader->FirstEntryOffset;
    char *Current = Start;

    /* This specific block should fit inside a cluster, if it doesn't, something is wrong with
       the FS! */
    if (HeaderStart + IndexHeader->FirstEntryOffset + IndexHeader->TotalEntriesSize >
        (char *)FsContext->ClusterBuffer + FsContext->BytesPerCluster) {
        *LastEntry = NULL;
        return NULL;
    }

    while (Current - Start < (ptrdiff_t)IndexHeader->TotalEntriesSize) {
        if (*CurrentIndex > Index) {
            return NULL;
        }

        NtfsIndexRecord *IndexEntry = (NtfsIndexRecord *)Current;
        uint16_t *NamePos = (uint16_t *)(Current + sizeof(NtfsIndexRecord));

        if (IndexEntry->Flags & 0x02) {
            break;
        } else if (
            *NamePos == '$' || (*NamePos == '.' && IndexEntry->NameLength == 1) ||
            (*CurrentIndex)++ != Index) {
            Current += IndexEntry->EntryLength;
            continue;
        }

        char *Name = BmAllocateZeroBlock(IndexEntry->NameLength + 1, 1);
        if (!Name) {
            /* This should make the caller return NULL too. */
            *LastEntry = NULL;
            return NULL;
        }

        for (int i = 0; i < IndexEntry->NameLength; i++) {
            Name[i] = *(NamePos++);
        }

        return Name;
    }

    *LastEntry = (NtfsIndexRecord *)Current;
    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cleans up any memory an NTFS file took while open.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to the NtfsContext struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void Close(void *ContextPtr) {
    RtSList *ListHeader = ((NtfsContext *)ContextPtr)->DataRunListHead.Next;
    while (ListHeader) {
        RtSList *Next = ListHeader->Next;
        BmFreeBlock(CONTAINING_RECORD(ListHeader, NtfsDataRun, ListHeader));
        ListHeader = Next;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be a NTFS file. Calling it on a directory will
 *     fail.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to NtfsContext struct.
 *     Offset - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int Read(void *ContextPtr, uint64_t Offset, uint64_t Size, void *Buffer) {
    const char ExpectedFileSignature[4] = "FILE";

    NtfsContext *Context = ContextPtr;
    NtfsFsContext *FsContext = Context->FsContext;
    NtfsMftEntry *MftEntry = FsContext->ClusterBuffer;

    if (Context->Directory || Offset + Size > Context->Size) {
        return 0;
    }

    if (!FsContext->ReadPartition(
            FsContext->Partition,
            FsContext->MftOffset + Context->MftEntry * FsContext->BytesPerMftEntry,
            FsContext->BytesPerMftEntry,
            FsContext->ClusterBuffer) ||
        memcmp(MftEntry->Signature, ExpectedFileSignature, 4) ||
        !ApplyFixups(FsContext, MftEntry->FixupOffset, MftEntry->NumberOfFixups)) {
        return 0;
    }

    /* The unnamed data stream (aka the main content stream) should have a type of 0x80. */
    uint64_t FirstVcn;
    uint64_t LastVcn;
    uint64_t Cluster;

    char *Current =
        FindAttribute(FsContext, 0x80, -1, &Context->DataRunListHead, &FirstVcn, &LastVcn);
    char *Output = Buffer;
    if (!Current) {
        return 0;
    }

    /* We're resident, yet, we somehow are over a cluster; Filesystem corruption, probably. */
    if (FirstVcn == UINT64_MAX &&
        Current + Offset >= (char *)FsContext->ClusterBuffer + FsContext->BytesPerCluster) {
        return 0;
    }

    /* Now safely seek through the file into the starting cluster (for the given byte index). */
    if (Offset > FsContext->BytesPerCluster) {
        size_t Clusters = Offset / FsContext->BytesPerCluster;

        FirstVcn += Clusters;
        Offset -= Clusters * FsContext->BytesPerCluster;

        if (FirstVcn > LastVcn || !TranslateVcn(&Context->DataRunListHead, FirstVcn, &Cluster) ||
            !FsContext->ReadPartition(
                FsContext->Partition,
                Cluster * FsContext->BytesPerCluster,
                FsContext->BytesPerCluster,
                FsContext->ClusterBuffer)) {
            return 0;
        }
    }

    while (Size) {
        size_t CopySize = FsContext->BytesPerCluster - Offset;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(Output, Current + Offset, CopySize);
        Output += CopySize;
        Offset = 0;
        Size -= CopySize;

        if (!Size) {
            break;
        }

        if (++FirstVcn > LastVcn || !TranslateVcn(&Context->DataRunListHead, FirstVcn, &Cluster) ||
            !FsContext->ReadPartition(
                FsContext->Partition,
                Cluster * FsContext->BytesPerCluster,
                FsContext->BytesPerCluster,
                FsContext->ClusterBuffer)) {
            return 0;
        }
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a NTFS directory (if its a file, we error
 *     out), searching for a specific node.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to the NtfsContext struct.
 *     Name - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *ReadEntry(void *ContextPtr, const char *Name) {
    const char ExpectedFileSignature[4] = "FILE";
    const char ExpectedIndexSignature[4] = "INDX";

    NtfsContext *Context = ContextPtr;
    NtfsFsContext *FsContext = Context->FsContext;
    NtfsMftEntry *MftEntry = FsContext->ClusterBuffer;

    if (!Context->Directory ||
        !FsContext->ReadPartition(
            FsContext->Partition,
            FsContext->MftOffset + Context->MftEntry * FsContext->BytesPerMftEntry,
            FsContext->BytesPerMftEntry,
            FsContext->ClusterBuffer) ||
        memcmp(MftEntry->Signature, ExpectedFileSignature, 4) ||
        !ApplyFixups(FsContext, MftEntry->FixupOffset, MftEntry->NumberOfFixups)) {
        return NULL;
    }

    /* $I30 ($INDEX_ROOT and $INDEX_ALLOCATION) are the attributes containing the directory
       tree itself (that we can iterate over). */
    NtfsIndexRootHeader *IndexRoot = FindAttribute(FsContext, 0x90, 0, NULL, NULL, NULL);
    if (!IndexRoot || IndexRoot->AttributeType != 0x30 || IndexRoot->CollationType != 0x01) {
        return NULL;
    }

    NtfsIndexRecord *LastEntry;
    BmFile *Handle =
        TraverseIndexBlockForName(FsContext, (char *)(IndexRoot + 1), &LastEntry, Name);
    if (Handle) {
        return Handle;
    } else if (!LastEntry || !(LastEntry->Flags & 0x01)) {
        return NULL;
    }

    /* We expect the "size" to be only one clusters. Other clusters are loaded by following the
       last index entry. */
    uint64_t FirstVcn;
    uint64_t LastVcn;
    uint64_t Cluster;
    NtfsIndexAllocationHeader *IndexAllocation =
        FindAttribute(FsContext, 0xA0, 1, &Context->DataRunListHead, &FirstVcn, &LastVcn);
    if (!IndexAllocation || FirstVcn != LastVcn) {
        return NULL;
    }

    while (1) {
        if (!ApplyFixups(
                FsContext, IndexAllocation->FixupOffset, IndexAllocation->NumberOfFixups) ||
            memcmp(IndexAllocation->Signature, ExpectedIndexSignature, 4) ||
            IndexAllocation->IndexVcn != FirstVcn) {
            return NULL;
        }

        Handle =
            TraverseIndexBlockForName(FsContext, (char *)(IndexAllocation + 1), &LastEntry, Name);
        if (Handle) {
            return Handle;
        } else if (!LastEntry || !(LastEntry->Flags & 0x01)) {
            return NULL;
        }

        FirstVcn = *(uint64_t *)((char *)LastEntry + LastEntry->EntryLength - 8);
        if (!TranslateVcn(&Context->DataRunListHead, FirstVcn, &Cluster) ||
            !FsContext->ReadPartition(
                FsContext->Partition,
                Cluster * FsContext->BytesPerCluster,
                FsContext->BytesPerCluster,
                FsContext->ClusterBuffer)) {
            return NULL;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the name of the Nth entry on what should be an NTFS directory (if its a
 *     file, we error out).
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to the NtfsContext struct.
 *     Index - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File name on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static char *Iterate(void *ContextPtr, int Index) {
    const char ExpectedFileSignature[4] = "FILE";
    const char ExpectedIndexSignature[4] = "INDX";

    NtfsContext *Context = ContextPtr;
    NtfsFsContext *FsContext = Context->FsContext;
    NtfsMftEntry *MftEntry = FsContext->ClusterBuffer;

    if (!Context->Directory ||
        !FsContext->ReadPartition(
            FsContext->Partition,
            FsContext->MftOffset + Context->MftEntry * FsContext->BytesPerMftEntry,
            FsContext->BytesPerMftEntry,
            FsContext->ClusterBuffer) ||
        memcmp(MftEntry->Signature, ExpectedFileSignature, 4) ||
        !ApplyFixups(FsContext, MftEntry->FixupOffset, MftEntry->NumberOfFixups)) {
        return NULL;
    }

    /* $I30 ($INDEX_ROOT and $INDEX_ALLOCATION) are the attributes containing the directory
       tree itself (that we can iterate over). */
    NtfsIndexRootHeader *IndexRoot = FindAttribute(FsContext, 0x90, 0, NULL, NULL, NULL);
    if (!IndexRoot || IndexRoot->AttributeType != 0x30 || IndexRoot->CollationType != 0x01) {
        return NULL;
    }

    int CurrentIndex = 0;
    NtfsIndexRecord *LastEntry;
    char *Name = TraverseIndexBlockForIndex(
        FsContext, (char *)(IndexRoot + 1), &LastEntry, &CurrentIndex, Index);
    if (Name) {
        return Name;
    } else if (!LastEntry || !(LastEntry->Flags & 0x01)) {
        return NULL;
    }

    /* We expect the "size" to be only one clusters. Other clusters are loaded by following the
       last index entry. */
    uint64_t FirstVcn;
    uint64_t LastVcn;
    uint64_t Cluster;
    NtfsIndexAllocationHeader *IndexAllocation =
        FindAttribute(FsContext, 0xA0, 1, &Context->DataRunListHead, &FirstVcn, &LastVcn);
    if (!IndexAllocation || FirstVcn != LastVcn) {
        return NULL;
    }

    while (1) {
        if (!ApplyFixups(
                FsContext, IndexAllocation->FixupOffset, IndexAllocation->NumberOfFixups) ||
            memcmp(IndexAllocation->Signature, ExpectedIndexSignature, 4) ||
            IndexAllocation->IndexVcn != FirstVcn) {
            return NULL;
        }

        Name = TraverseIndexBlockForIndex(
            FsContext, (char *)(IndexAllocation + 1), &LastEntry, &CurrentIndex, Index);
        if (Name) {
            return Name;
        } else if (!LastEntry || !(LastEntry->Flags & 0x01)) {
            return NULL;
        }

        FirstVcn = *(uint64_t *)((char *)LastEntry + LastEntry->EntryLength - 8);
        if (!TranslateVcn(&Context->DataRunListHead, FirstVcn, &Cluster) ||
            !FsContext->ReadPartition(
                FsContext->Partition,
                Cluster * FsContext->BytesPerCluster,
                FsContext->BytesPerCluster,
                FsContext->ClusterBuffer)) {
            return NULL;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a root directory file handle.
 *
 * PARAMETERS:
 *     ContextPtr - NTFS-specific partition data.
 *
 * RETURN VALUE:
 *     New handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *OpenRoot(void *ContextPtr) {
    NtfsFsContext *FsContext = ContextPtr;
    BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(NtfsContext));
    if (!Handle) {
        return NULL;
    }

    /* MFT entry 5 is `.` (the root directory). */
    NtfsContext *Context = (NtfsContext *)(Handle + 1);
    Context->FsContext = FsContext;
    Context->DataRunListHead.Next = NULL;
    Context->MftEntry = 5;
    Context->Size = 0;
    Context->Directory = 1;

    Handle->Size = 0;
    Handle->Context = Context;
    Handle->Close = Close;
    Handle->Read = Read;
    Handle->ReadEntry = ReadEntry;
    Handle->Iterate = Iterate;

    return Handle;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries probing an open device/partition for NTFS.
 *
 * PARAMETERS:
 *     Partition - Which partition we're checking.
 *     ReadPartition - Function to read some bytes from the partition.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeNtfs(BmPartition *Partition, BmFileReadFn ReadPartition) {
    const char ExpectedFileSystemName[8] = "NTFS    ";

    char *Buffer = BmAllocateBlock(512);
    NtfsBootSector *BootSector = (NtfsBootSector *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* Using the FileSystemName and the expected value for all fields, we should be able to
       somewhat confidently tell if this is NTFS or not.
       We're using the documentation at
       https://github.com/libyal/libfsntfs/blob/main/documentation/New%20Technologies%20File%20System%20(NTFS).asciidoc
     */

    if (!ReadPartition(Partition, 0, 512, Buffer) ||
        memcmp(BootSector->FileSystemName, ExpectedFileSystemName, 8) ||
        !(BootSector->BytesPerSector >= 256 && BootSector->BytesPerSector <= 4096) ||
        BootSector->SectorsPerCluster > 255 || BootSector->ReservedSectors ||
        BootSector->NumberOfFats || BootSector->RootEntries || BootSector->NumberOfSectors16 ||
        BootSector->SectorsPerFat || BootSector->NumberOfSectors32 ||
        BootSector->BpbSignature != 0x80 || BootSector->MftEntrySize > 255 ||
        BootSector->IndexEntrySize > 255) {
        BmFreeBlock(Buffer);
        return 0;
    }

    /* We can be somewhat confident now that the FS at least looks like NTFS; either the
       user wants to open the root directory (and leave at that), or open something inside it;
       either way, save the root directory to the new context struct. */
    NtfsFsContext *FsContext = BmAllocateBlock(sizeof(NtfsFsContext));
    if (!FsContext) {
        BmFreeBlock(Buffer);
        return 0;
    }

    FsContext->Partition = Partition;
    FsContext->ReadPartition = ReadPartition;
    FsContext->BytesPerSector = BootSector->BytesPerSector;
    FsContext->BytesPerCluster = BootSector->BytesPerSector;
    if (BootSector->SectorsPerCluster > 243) {
        FsContext->BytesPerCluster *= 1 << (256 - BootSector->SectorsPerCluster);
    } else {
        FsContext->BytesPerCluster *= BootSector->SectorsPerCluster;
    }

    FsContext->ClusterBuffer = BmAllocateBlock(FsContext->BytesPerCluster);
    if (!FsContext->ClusterBuffer) {
        BmFreeBlock(FsContext);
        BmFreeBlock(Buffer);
        return 0;
    }

    if (BootSector->MftEntrySize > 127) {
        FsContext->BytesPerMftEntry = 1 << -(int8_t)BootSector->MftEntrySize;
    } else {
        FsContext->BytesPerMftEntry = FsContext->BytesPerCluster * BootSector->MftEntrySize;
    }

    FsContext->MftOffset = BootSector->MftCluster * FsContext->BytesPerCluster;
    Partition->FsContext = FsContext;
    Partition->OpenRoot = OpenRoot;

    BmFreeBlock(Buffer);
    return 1;
}
