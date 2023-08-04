/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _EXFAT_H_
#define _EXFAT_H_

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    char JumpBoot[3];
    char FileSystemName[8];
    char MustBeZero[53];
    uint64_t PartitionOffset;
    uint64_t VolumeLength;
    uint32_t FatOffset;
    uint32_t FatLength;
    uint32_t ClusterHeapOffset;
    uint32_t ClusterCount;
    uint32_t FirstClusterOfRootDirectory;
    uint32_t VolumeSerialNumber;
    uint16_t FileSystemRevision;
    uint16_t VolumeFlags;
    uint8_t BytesPerSectorShift;
    uint8_t SectorsPerClusterShift;
    uint8_t NumberOfFats;
    uint8_t DriveSelect;
    uint8_t PercentInUse;
    char Reserved[7];
    char BootCode[390];
    uint16_t BootSignature;
} ExfatBootSector;

typedef struct __attribute__((packed)) {
    uint8_t EntryType;
    char CustomDefined[19];
    uint32_t FirstCluster;
    uint64_t DataLength;
} ExfatGenericDirectoryEntry;

#endif /* _EXFAT_H_ */
