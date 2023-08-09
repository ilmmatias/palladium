/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <file.h>
#include <ntfs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct NtfsDataRun {
    int Used;
    uint64_t Vcn;
    uint64_t Lcn;
    uint64_t Length;
    struct NtfsDataRun *Next;
} NtfsDataRun;

typedef struct {
    FileContext Parent;
    void *ClusterBuffer;
    NtfsDataRun *DataRuns;
    uint16_t BytesPerSector;
    uint32_t BytesPerCluster;
    uint32_t BytesPerMftEntry;
    uint32_t BytesPerIndexEntry;
    uint64_t MftOffset;
    uint64_t FileEntry;
    uint64_t FileSize;
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
    FsContext->FileSize = 0;
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
 *     This function uses a data run list obtained from FindAttribute to convert a Virtual Cluster
 *     Number (VCN) into a Logical Cluster Number (LCN, aka something we can just multiply by the
 *     size of a cluster and __fread).
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     Vcn - Virtual cluster to translate.
 *     Lcn - Output; Contains the Logical Cluster Number on success.
 *
 * RETURN VALUE:
 *     1 on match (if a translation was found), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int TranslateVcn(NtfsContext *FsContext, uint64_t Vcn, uint64_t *Lcn) {
    for (NtfsDataRun *Entry = FsContext->DataRuns; Entry && Entry->Used; Entry = Entry->Next) {
        if (Entry->Vcn >= Vcn) {
            *Lcn = Entry->Lcn + Vcn - Entry->Vcn;
            return 1;
        }
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
    NtfsContext *FsContext,
    uint32_t Type,
    int NonResident,
    uint64_t *FirstVcn,
    uint64_t *LastVcn) {
    char *Start = FsContext->ClusterBuffer;
    char *Current = Start + ((NtfsMftEntry *)FsContext->ClusterBuffer)->AttributeOffset;

    while (Current - Start < (ptrdiff_t)FsContext->BytesPerCluster) {
        NtfsMftAttributeHeader *Attribute = (NtfsMftAttributeHeader *)Current;

        if (Attribute->Type == Type) {
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

            /* On non-resident attributes, we always load up the first cluster (saving the
               last cluster); This will invalidate all pointers we currently have into the
               cluster buffer, make sure to copy any important data beforehand (such as we do for
               the data run translations themselves)! */
            uint8_t *DataRun = (uint8_t *)Current + Attribute->NonResidentForm.DataRunOffset;
            NtfsDataRun *LastRun = FsContext->DataRuns;
            NtfsDataRun *CurrentRun = LastRun;
            uint64_t CurrentVcn = 0;

            while (1) {
                uint8_t Current = *(DataRun++);
                uint8_t OffsetSize = Current >> 4;
                uint8_t LengthSize = Current & 0x0F;

                if (!Current) {
                    if (CurrentRun) {
                        CurrentRun->Used = 0;
                    }

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
                NtfsDataRun *Entry = CurrentRun;
                if (!Entry) {
                    Entry = malloc(sizeof(NtfsDataRun));
                    if (!Entry) {
                        return NULL;
                    }
                }

                Entry->Used = 1;
                Entry->Vcn = CurrentVcn;
                Entry->Lcn = Offset;
                Entry->Length = Length;

                if (!CurrentRun && !LastRun) {
                    FsContext->DataRuns = Entry;
                } else if (!CurrentRun) {
                    LastRun->Next = Entry;
                } else {
                    CurrentRun = CurrentRun->Next;
                }

                LastRun = Entry;
                CurrentVcn += Length;
            }

            if (FirstVcn) {
                *FirstVcn = Attribute->NonResidentForm.FirstVcn;
            }

            if (LastVcn) {
                *LastVcn = Attribute->NonResidentForm.LastVcn;
            }

            uint64_t Cluster;
            if (!TranslateVcn(FsContext, Attribute->NonResidentForm.FirstVcn, &Cluster) ||
                __fread(
                    &FsContext->Parent,
                    Cluster * FsContext->BytesPerCluster,
                    FsContext->ClusterBuffer,
                    FsContext->BytesPerCluster,
                    NULL)) {
                return NULL;
            }

            return FsContext->ClusterBuffer;
        }

        Current += Attribute->Size;
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
 *     Either a pointer to the attribute, or NULL if it couldn't be found.
 *-----------------------------------------------------------------------------------------------*/
static int TraverseIndexBlock(
    NtfsContext *FsContext,
    char *HeaderStart,
    NtfsIndexRecord **LastEntry,
    const char *Name) {
    NtfsIndexHeader *IndexHeader = (NtfsIndexHeader *)HeaderStart;
    char *Start = HeaderStart + IndexHeader->FirstEntryOffset;
    char *Current = Start;

    /* This specific block should fit inside a cluster, if it doesn't, something is wrong with
       the FS! */
    if (HeaderStart + IndexHeader->FirstEntryOffset + IndexHeader->TotalEntriesSize >
        (char *)FsContext->ClusterBuffer + FsContext->BytesPerCluster) {
        *LastEntry = NULL;
        return 0;
    }

    while (Current - Start < (ptrdiff_t)IndexHeader->TotalEntriesSize) {
        NtfsIndexRecord *IndexEntry = (NtfsIndexRecord *)Current;
        uint16_t *ThisNamePos = (uint16_t *)(Current + sizeof(NtfsIndexRecord));
        const char *SearchNamePos = Name;
        int Match = 1;

        if (IndexEntry->Flags & 0x02) {
            break;
        }

        while (*SearchNamePos && IndexEntry->NameLength) {
            if (*(ThisNamePos++) != *(SearchNamePos++)) {
                Match = 0;
                break;
            }

            IndexEntry->NameLength--;
        }

        if (Match && !IndexEntry->NameLength) {
            FsContext->FileEntry = IndexEntry->MftEntry & 0xFFFFFFFFFFFF;
            FsContext->Directory = (IndexEntry->FileFlags & 0x10000000) != 0;
            FsContext->FileSize = IndexEntry->RealSize;
            return 1;
        }

        Current += IndexEntry->EntryLength;
    }

    *LastEntry = (NtfsIndexRecord *)Current;
    return 0;
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
    const char ExpectedFileSignature[4] = "FILE";
    const char ExpectedIndexSignature[4] = "INDX";

    NtfsContext *FsContext = Context->PrivateData;
    NtfsMftEntry *MftEntry = FsContext->ClusterBuffer;

    if (!FsContext->Directory ||
        __fread(
            &FsContext->Parent,
            FsContext->MftOffset + FsContext->FileEntry * FsContext->BytesPerMftEntry,
            FsContext->ClusterBuffer,
            FsContext->BytesPerCluster,
            NULL) ||
        memcmp(MftEntry->Signature, ExpectedFileSignature, 4) ||
        !ApplyFixups(FsContext, MftEntry->FixupOffset, MftEntry->NumberOfFixups)) {
        return 0;
    }

    /* $I30 ($INDEX_ROOT and $INDEX_ALLOCATION) are the attributes containing the directory
       tree itself (that we can iterate over). */
    NtfsIndexRootHeader *IndexRoot = FindAttribute(FsContext, 0x90, 0, NULL, NULL);
    if (!IndexRoot || IndexRoot->AttributeType != 0x30 || IndexRoot->CollationType != 0x01) {
        return 0;
    }

    NtfsIndexRecord *LastEntry;
    if (TraverseIndexBlock(FsContext, (char *)(IndexRoot + 1), &LastEntry, Name)) {
        return 1;
    } else if (!LastEntry || !(LastEntry->Flags & 0x01)) {
        return 0;
    }

    /* We expect the "size" to be only one clusters. Other clusters are loaded by following the
       last index entry. */
    uint64_t FirstVcn;
    uint64_t LastVcn;
    uint64_t Cluster;
    NtfsIndexAllocationHeader *IndexAllocation =
        FindAttribute(FsContext, 0xA0, 1, &FirstVcn, &LastVcn);
    if (!IndexAllocation || FirstVcn != LastVcn) {
        return 0;
    }

    while (1) {
        if (!ApplyFixups(
                FsContext, IndexAllocation->FixupOffset, IndexAllocation->NumberOfFixups) ||
            memcmp(IndexAllocation->Signature, ExpectedIndexSignature, 4) ||
            IndexAllocation->IndexVcn != FirstVcn) {
            return 0;
        }

        if (TraverseIndexBlock(FsContext, (char *)(IndexAllocation + 1), &LastEntry, Name)) {
            return 1;
        } else if (!LastEntry || !(LastEntry->Flags & 0x01)) {
            return 0;
        }

        FirstVcn = *(uint64_t *)((char *)LastEntry + 8 - LastEntry->EntryLength);
        if (!TranslateVcn(FsContext, FirstVcn, &Cluster) ||
            __fread(
                &FsContext->Parent,
                Cluster * FsContext->BytesPerCluster,
                FsContext->ClusterBuffer,
                FsContext->BytesPerCluster,
                NULL)) {
            return 0;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be an NTFS file. Calling it on a directory will
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
int BiReadNtfsFile(FileContext *Context, void *Buffer, size_t Start, size_t Size, size_t *Read) {
    const char ExpectedFileSignature[4] = "FILE";

    NtfsContext *FsContext = Context->PrivateData;
    NtfsMftEntry *MftEntry = FsContext->ClusterBuffer;
    size_t Accum = 0;
    int Flags = 0;

    if (FsContext->Directory) {
        return __STDIO_FLAGS_ERROR;
    } else if (Start > FsContext->FileSize) {
        return __STDIO_FLAGS_EOF;
    }

    if (Size > FsContext->FileSize - Start) {
        Flags = __STDIO_FLAGS_EOF;
        Size = FsContext->FileSize - Start;
    }

    if (__fread(
            &FsContext->Parent,
            FsContext->MftOffset + FsContext->FileEntry * FsContext->BytesPerMftEntry,
            FsContext->ClusterBuffer,
            FsContext->BytesPerCluster,
            NULL) ||
        memcmp(MftEntry->Signature, ExpectedFileSignature, 4) ||
        !ApplyFixups(FsContext, MftEntry->FixupOffset, MftEntry->NumberOfFixups)) {
        return __STDIO_FLAGS_ERROR;
    }

    /* The unnamed data stream (aka the main content stream) should have a type of 0x80. */
    uint64_t FirstVcn;
    uint64_t LastVcn;
    uint64_t Cluster;

    char *Current = FindAttribute(FsContext, 0x80, -1, &FirstVcn, &LastVcn);
    char *Output = Buffer;
    if (!Current) {
        return __STDIO_FLAGS_ERROR;
    }

    /* We're resident, yet, we somehow are over a cluster; Filesystem corruption, probably. */
    if (FirstVcn == UINT64_MAX &&
        Current + Start >= (char *)FsContext->ClusterBuffer + FsContext->BytesPerCluster) {
        return __STDIO_FLAGS_ERROR;
    }

    /* Now safely seek through the file into the starting cluster (for the given byte index). */
    while (Start > FsContext->BytesPerCluster) {
        if (++FirstVcn > LastVcn || !TranslateVcn(FsContext, FirstVcn, &Cluster) ||
            __fread(
                &FsContext->Parent,
                Cluster * FsContext->BytesPerCluster,
                FsContext->ClusterBuffer,
                FsContext->BytesPerCluster,
                NULL)) {
            return __STDIO_FLAGS_ERROR;
        }

        Start -= FsContext->BytesPerCluster;
    }

    while (1) {
        size_t CopySize = FsContext->BytesPerCluster - Start;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(Output, Current + Start, CopySize);
        Output += CopySize;
        Start += CopySize;
        Size -= CopySize;
        Accum += CopySize;

        if (!Size) {
            break;
        }

        if (++FirstVcn > LastVcn || !TranslateVcn(FsContext, FirstVcn, &Cluster) ||
            __fread(
                &FsContext->Parent,
                Cluster * FsContext->BytesPerCluster,
                FsContext->ClusterBuffer,
                FsContext->BytesPerCluster,
                NULL)) {
            if (Read) {
                *Read = Accum;
            }

            return __STDIO_FLAGS_ERROR;
        }
    }

    if (Read) {
        *Read = Accum;
    }

    return Flags;
}
