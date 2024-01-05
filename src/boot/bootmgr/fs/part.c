/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <file.h>
#include <memory.h>
#include <stdlib.h>

BmPartition *BiBootPartition = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads data from a partition.
 *
 * PARAMETERS:
 *     Context - Expected to be a valid BmPartition struct.
 *     Offset - Starting byte index (into the partition); This will be further offset by
 *              Context->Offset.
 *     Size - How many bytes to read into the buffer.
 *     Buffer - Output buffer.
 *
 * RETURN VALUE:
 *     0 if something went wrong, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ReadRawPartition(void *Context, uint64_t Offset, uint64_t Size, void *Buffer) {
    BmPartition *Partition = Context;
    return Partition->ReadDisk(Partition->DeviceContext, Partition->Offset + Offset, Size, Buffer);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function probes for partitions inside the given disk.
 *
 * PARAMETERS:
 *     ListHead - Partition list associated with the device (we'll be filling it).
 *     ReadDisk - Read disk routine.
 *     Context - Context associated with the device (required for reads).
 *     SectorSize - Size of a sector in bytes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiProbeDisk(RtSList *ListHead, BmFileReadFn ReadDisk, void *Context, uint64_t SectorSize) {
    BiProbePartition(Context, ReadDisk);
    BiProbeMbrDisk(ListHead, ReadDisk, Context, SectorSize);

    /* Probe for filesystems inside each partition; If this fails, the partition can only be read
       raw/without OpenRoot. */
    for (ListHead = ListHead->Next; ListHead; ListHead = ListHead->Next) {
        BiProbePartition(CONTAINING_RECORD(ListHead, BmPartition, ListHeader), ReadRawPartition);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function probes the specified device for any supported filesystem.
 *
 * PARAMETERS:
 *     Partition - The device handle we're probing.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
int BiProbePartition(BmPartition *Partition, BmFileReadFn ReadPartition) {
    /* The order of probing somewhat matters, as some of those are more prone to misdetects
       (false positives). */
    return BiProbeIso9660(Partition, ReadPartition) || BiProbeExfat(Partition, ReadPartition) ||
           BiProbeNtfs(Partition, ReadPartition) || BiProbeFat32(Partition, ReadPartition);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function opens the given partition inside a disk.
 *
 * PARAMETERS:
 *     ListHead - Partition list associated with the device.
 *     Name - Which partition we want to open (part(N)).
 *
 * RETURN VALUE:
 *     Handle to the partition, or NULL if we failed to open it.
 *-----------------------------------------------------------------------------------------------*/
BmFile *BiOpenPartition(RtSList *ListHead, const char *Name) {
    int Index = 0;

    if (tolower(*Name) != 'p' || tolower(*(Name + 1)) != 'a' || tolower(*(Name + 2)) != 'r' ||
        tolower(*(Name + 3)) != 't' || *(Name + 4) != '(') {
        return NULL;
    } else {
        Index = strtol(Name + 5, NULL, 16);
    }

    ListHead = ListHead->Next;
    while (ListHead) {
        BmPartition *Partition = CONTAINING_RECORD(ListHead, BmPartition, ListHeader);

        if (Partition->Index == Index) {
            BmFile *Handle = BmAllocateBlock(sizeof(BmFile));
            if (!Handle) {
                return NULL;
            }

            Handle->Size = 0;
            Handle->Context = Partition;
            Handle->Close = NULL;
            Handle->Read = ReadRawPartition;
            Handle->ReadEntry = NULL;
            Handle->Iterate = NULL;

            return Handle;
        }

        ListHead = ListHead->Next;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function opens the partition from which we booted.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Handle to the partition, or NULL if we failed to open it.
 *-----------------------------------------------------------------------------------------------*/
BmFile *BiOpenBootPartition(void) {
    BmFile *Handle = BmAllocateBlock(sizeof(BmFile));
    if (!Handle) {
        return NULL;
    }

    Handle->Size = 0;
    Handle->Context = BiBootPartition;
    Handle->Close = NULL;
    Handle->Read = ReadRawPartition;
    Handle->ReadEntry = NULL;
    Handle->Iterate = NULL;

    return Handle;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries opening the root directory of the specified device/partition.
 *     This will fail if there was no detected filesystem.
 *
 * PARAMETERS:
 *     Partition - Which device we're trying to open.
 *
 * RETURN VALUE:
 *     Handle to the root directory, or NULL if we failed to open it.
 *-----------------------------------------------------------------------------------------------*/
BmFile *BiOpenRoot(BmPartition *Partition) {
    return Partition->OpenRoot ? Partition->OpenRoot(Partition->FsContext) : NULL;
}
