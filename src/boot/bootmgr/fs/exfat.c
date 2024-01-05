/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <exfat.h>
#include <file.h>
#include <memory.h>
#include <string.h>

typedef struct {
    BmPartition *Partition;
    BmFileReadFn ReadPartition;
    void *ClusterBuffer;
    uint8_t ClusterShift;
    uint8_t SectorShift;
    uint64_t ClusterOffset;
    uint64_t FatOffset;
    uint64_t RootCluster;
    int RootNoFatChain;
} ExfatFsContext;

typedef struct {
    ExfatFsContext *FsContext;
    uint64_t FileCluster;
    uint64_t Size;
    int NoFatChain;
    int Directory;
} ExfatContext;

static char *Iterate(void *ContextPtr, int Index);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if we need to read in a cluster, following the FAT if required.
 *
 * PARAMETERS:
 *     FsContext - FS-specific data.
 *     Current - I/O; Current position along the cluster buffer.
 *     Cluster - I/O; Current cluster (minus 2, as usual on FAT32/exFAT).
 *     NoFatChain - Set this to 1 if the file's no fat chain flag was set.
 *
 * RETURN VALUE:
 *     1 for success, otherwise 0.
 *-----------------------------------------------------------------------------------------------*/
static int
FollowCluster(ExfatFsContext *FsContext, void **Current, uint64_t *Cluster, int NoFatChain) {
    ptrdiff_t Offset = Current ? *Current - FsContext->ClusterBuffer : 0;

    if (Offset && Offset < (1 << FsContext->ClusterShift)) {
        return 1;
    } else if (Offset || !Current) {
        if (NoFatChain) {
            (*Cluster)++;
        } else if (!FsContext->ReadPartition(
                       FsContext->Partition,
                       FsContext->FatOffset + (*Cluster << 2),
                       4,
                       FsContext->ClusterBuffer)) {
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
        return FsContext->ReadPartition(
            FsContext->Partition,
            FsContext->ClusterOffset + ((*Cluster - 2) << FsContext->ClusterShift),
            1 << FsContext->ClusterShift,
            FsContext->ClusterBuffer);
    } else {
        return 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to read what should be an exFAT file. Calling it on a directory will
 *     fail.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to ExfatContext struct.
 *     Offset - Starting byte index (in the file).
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int Read(void *ContextPtr, uint64_t Offset, uint64_t Size, void *Buffer) {
    ExfatContext *Context = ContextPtr;
    ExfatFsContext *FsContext = Context->FsContext;
    uint64_t Cluster = Context->FileCluster;
    char *Current = FsContext->ClusterBuffer;
    char *Output = Buffer;

    if (Context->Directory || Offset + Size > Context->Size) {
        return 0;
    }

    /* Seek through the file into the starting cluster (for the given byte index). */
    while (Offset >= (1 << FsContext->ClusterShift)) {
        Offset -= 1 << FsContext->ClusterShift;
        if (!FollowCluster(FsContext, NULL, &Cluster, Context->NoFatChain)) {
            return 0;
        }
    }

    while (Size) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
            return 0;
        }

        uint64_t CopySize = (1 << FsContext->ClusterShift) - Offset;
        if (Size < CopySize) {
            CopySize = Size;
        }

        memcpy(Output, Current + Offset, CopySize);
        Current += 1 << FsContext->ClusterShift;
        Output += CopySize;
        Offset = 0;
        Size -= CopySize;
    }

    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does traversal on what should be a exFAT directory (if its a file, we error
 *     out), searching for a specific node.
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to the ExfatContext struct.
 *     Name - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *ReadEntry(void *ContextPtr, const char *Name) {
    ExfatContext *Context = ContextPtr;
    ExfatFsContext *FsContext = Context->FsContext;
    ExfatDirectoryEntry *Current = FsContext->ClusterBuffer;
    uint64_t Cluster = Context->FileCluster;
    size_t NameLength = strlen(Name);

    if (!Context->Directory) {
        return NULL;
    }

    while (1) {
        if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain) ||
            !Current->EntryType) {
            return NULL;
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

        if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
            return NULL;
        } else if (
            Current->EntryType != 0xC0 || ((ExfatStreamEntry *)Current)->NameLength != NameLength) {
            while (Entry.SecondaryCount--) {
                Current++;
                if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
                    return NULL;
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
            if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
                return NULL;
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
                if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
                    return NULL;
                }
            }

            continue;
        }

        /* Name match, setup the file entry to reflect the new file/directory we opened, and
           we're done. */
        BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(ExfatContext));
        if (!Handle) {
            return NULL;
        }

        Context = (ExfatContext *)(Handle + 1);
        Context->FsContext = FsContext;
        Context->FileCluster = StreamEntry.FirstCluster;
        Context->Size = StreamEntry.ValidDataLength;
        Context->NoFatChain = StreamEntry.GeneralSecondaryFlags & 0x02;
        Context->Directory = Entry.FileAttributes & 0x10;

        Handle->Size = StreamEntry.ValidDataLength;
        Handle->Context = Context;
        Handle->Close = NULL;
        Handle->Read = Read;
        Handle->ReadEntry = ReadEntry;
        Handle->Iterate = Iterate;

        return Handle;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the name of the Nth entry on what should be an exFAT directory (if its a
 *     file, we error out).
 *
 * PARAMETERS:
 *     ContextPtr - Pointer to the ExfatContext struct.
 *     Index - Which entry we're looking for.
 *
 * RETURN VALUE:
 *     File name on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static char *Iterate(void *ContextPtr, int Index) {
    ExfatContext *Context = ContextPtr;
    ExfatFsContext *FsContext = Context->FsContext;
    ExfatDirectoryEntry *Current = FsContext->ClusterBuffer;
    uint64_t Cluster = Context->FileCluster;

    if (!Context->Directory) {
        return NULL;
    }

    int CurrentIndex = 0;
    while (1) {
        if (CurrentIndex > Index) {
            return NULL;
        } else if (
            !FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain) ||
            !Current->EntryType) {
            return NULL;
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

        if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
            return NULL;
        } else if (Current->EntryType != 0xC0 || CurrentIndex++ != Index) {
            while (Entry.SecondaryCount--) {
                Current++;
                if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
                    return NULL;
                }
            }

            continue;
        }

        ExfatStreamEntry StreamEntry;
        memcpy(&StreamEntry, Current, sizeof(ExfatStreamEntry));

        /* File Name entries should only be valid directly following the stream entry. */
        char *Name = BmAllocateZeroBlock(StreamEntry.NameLength + 1, 1);
        if (!Name) {
            return NULL;
        }

        char *NamePos = Name;
        Current++;

        while (StreamEntry.NameLength) {
            if (!FollowCluster(FsContext, (void **)&Current, &Cluster, Context->NoFatChain)) {
                BmFreeBlock(Name);
                return NULL;
            } else if (Current->EntryType != 0xC1) {
                Entry.SecondaryCount--;
                break;
            }

            ExfatFileNameEntry *NameEntry = (ExfatFileNameEntry *)Current;

            for (int i = 0; i < (StreamEntry.NameLength < 15 ? StreamEntry.NameLength : 15); i++) {
                *(NamePos++) = NameEntry->FileName[i];
            }

            Current++;
            Entry.SecondaryCount--;
            StreamEntry.NameLength = StreamEntry.NameLength > 15 ? StreamEntry.NameLength - 15 : 0;
        }

        return Name;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a root directory file handle.
 *
 * PARAMETERS:
 *     ContextPtr - exFAT-specific partition data.
 *
 * RETURN VALUE:
 *     New handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
static BmFile *OpenRoot(void *ContextPtr) {
    ExfatFsContext *FsContext = ContextPtr;
    BmFile *Handle = BmAllocateBlock(sizeof(BmFile) + sizeof(ExfatContext));
    if (!Handle) {
        return NULL;
    }

    ExfatContext *Context = (ExfatContext *)(Handle + 1);
    Context->FsContext = FsContext;
    Context->FileCluster = FsContext->RootCluster;
    Context->Size = 0;
    Context->NoFatChain = FsContext->RootNoFatChain;
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
 *     This function tries probing an open device/partition for exFAT.
 *
 * PARAMETERS:
 *     Partition - Which partition we're checking.
 *     ReadPartition - Function to read some bytes from the partition.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiProbeExfat(BmPartition *Partition, BmFileReadFn ReadPartition) {
    const char ExpectedJumpBoot[3] = {0xEB, 0x76, 0x90};
    const char ExpectedFileSystemName[8] = "EXFAT   ";

    char *Buffer = BmAllocateBlock(512);
    ExfatBootSector *BootSector = (ExfatBootSector *)Buffer;
    if (!Buffer) {
        return 0;
    }

    /* We can use the mandatory fields described on the Main Boot Sector structure at
       https://learn.microsoft.com/en-us/windows/win32/fileio/exfat-specification to quickly do
       the probing. */

    if (!ReadPartition(Partition, 0, 512, Buffer) ||
        memcmp(BootSector->JumpBoot, ExpectedJumpBoot, 3) ||
        memcmp(BootSector->FileSystemName, ExpectedFileSystemName, 8)) {
        extern void BmPrint(const char *, ...);
        BmFreeBlock(Buffer);
        return 0;
    }

    for (int i = 0; i < 53; i++) {
        if (BootSector->MustBeZero[i]) {
            BmFreeBlock(Buffer);
            return 0;
        }
    }

    /* We can be somewhat confident now that the FS at least looks like exFAT; either the
       user wants to open the root directory (and leave at that), or open something inside it;
       either way, save the root directory to the new context struct. */
    ExfatFsContext *FsContext = BmAllocateBlock(sizeof(ExfatFsContext));
    if (!FsContext) {
        BmFreeBlock(Buffer);
        return 0;
    }

    FsContext->Partition = Partition;
    FsContext->ReadPartition = ReadPartition;
    FsContext->ClusterShift = BootSector->BytesPerSectorShift + BootSector->SectorsPerClusterShift;
    FsContext->ClusterBuffer = BmAllocateBlock(1 << FsContext->ClusterShift);
    if (!FsContext->ClusterBuffer) {
        BmFreeBlock(FsContext);
        BmFreeBlock(Buffer);
        return 0;
    }

    FsContext->SectorShift = BootSector->BytesPerSectorShift;
    FsContext->ClusterOffset = BootSector->ClusterHeapOffset << BootSector->BytesPerSectorShift;
    FsContext->FatOffset = BootSector->FatOffset << BootSector->BytesPerSectorShift;
    FsContext->RootCluster = BootSector->FirstClusterOfRootDirectory;

    /* Grab the NoFatChain flag from the root directory (the first entry of a directory
       tells us that info). */
    if (!ReadPartition(
            Partition,
            FsContext->ClusterOffset + ((FsContext->RootCluster - 2) << FsContext->ClusterShift),
            sizeof(ExfatDirectoryEntry),
            FsContext->ClusterBuffer)) {
        BmFreeBlock(FsContext);
        BmFreeBlock(Buffer);
        return 0;
    }

    Partition->FsContext = FsContext;
    Partition->OpenRoot = OpenRoot;

    BmFreeBlock(Buffer);
    return 1;
}
