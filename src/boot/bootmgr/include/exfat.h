/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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
    uint8_t SecondaryCount;
    uint16_t SetChecksum;
    uint16_t FileAttributes;
    char Reserved1[2];
    uint32_t CreateTimestamp;
    uint32_t LastModifiedTimestamp;
    uint32_t LastAccessedTimestamp;
    uint8_t Create10msIncrement;
    uint8_t LastModified10msIncrement;
    uint8_t CreateUtcOffset;
    uint8_t LastModifiedUtcOffset;
    uint8_t LastAccessedUtcOffset;
    char Reserved2[7];
} ExfatDirectoryEntry;

typedef struct __attribute__((packed)) {
    uint8_t EntryType;
    uint8_t GeneralSecondaryFlags;
    char Reserved1;
    uint8_t NameLength;
    uint16_t NameHash;
    char Reserved2[2];
    uint64_t ValidDataLength;
    char Reserved3[4];
    uint32_t FirstCluster;
    uint64_t DataLength;
} ExfatStreamEntry;

typedef struct __attribute__((packed)) {
    uint8_t EntryType;
    uint8_t GeneralSecondaryFlags;
    uint16_t FileName[15];
} ExfatFileNameEntry;

#endif /* _EXFAT_H_ */
