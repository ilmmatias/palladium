/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <file.h>
#include <memory.h>
#include <string.h>

typedef struct __attribute__((packed)) {
    uint8_t Flags;
    uint8_t ChsStart[3];
    uint8_t Type;
    uint8_t ChsEnd[3];
    uint32_t FirstSector;
    uint32_t Sectors;
} PartitionRecord;

typedef struct __attribute__((packed)) {
    char Bootstrap[440];
    uint32_t DiskId;
    uint16_t Reserved;
    PartitionRecord Partitions[4];
    uint8_t Signature[2];
} BootRecord;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function probes the given disk for MBR partitions.
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
void BiProbeMbrDisk(RtSList *ListHead, BmFileReadFn ReadDisk, void *Context, uint64_t SectorSize) {
    BootRecord Record;
    if (!ReadDisk(Context, 0, 512, &Record)) {
        return;
    }

    int Entry = 0;
    for (int i = 0; i < 4; i++) {
        if (!Record.Partitions[i].Type) {
            continue;
        }

        BmPartition *Partition = BmAllocateBlock(sizeof(BmPartition));
        if (!Partition) {
            Entry++;
            continue;
        }

        Partition->Index = Entry++;
        Partition->Active = Record.Partitions[i].Flags & 0x80;
        Partition->Offset = Record.Partitions[i].FirstSector * SectorSize;
        Partition->DeviceContext = Context;
        Partition->ReadDisk = ReadDisk;

        RtPushSList(ListHead, &Partition->ListHeader);
    }
}
