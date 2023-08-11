/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _FAT32_H_
#define _FAT32_H_

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    char JumpBoot[3];
    char FileSystemName[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t NumberOfFats;
    uint16_t NumberOfRootEntries;
    uint16_t SmallTotalSectors;
    uint8_t Media;
    uint16_t SectorsPerFat16;
    uint16_t SectorsPerTrack;
    uint16_t Heads;
    uint32_t HiddenSectors;
    uint32_t LargeTotalSectors;
    uint32_t SectorsPerFat;
    uint16_t Flags;
    uint16_t FatVersion;
    uint32_t RootDirectoryCluster;
    uint16_t FsInfoSector;
    uint16_t BackupBootSectorSector;
    char Reserved1[12];
    uint8_t DriveNumber;
    char Reserved2;
    uint8_t Signature;
    uint32_t SerialNumber;
    char VolumeLabel[11];
    char SystemIdentifier[8];
    char BootCode[420];
    uint16_t BootSignature;
} Fat32BootSector;

typedef struct __attribute__((packed)) {
    char DosName[11];
    uint8_t Attributes;
    char Reserved;
    uint8_t CreationTimeTenths;
    uint16_t CreationTime;
    uint16_t CreationDate;
    uint16_t LastAccessDate;
    uint16_t FileClusterHigh;
    uint16_t LastModificationTime;
    uint16_t LastModificationDate;
    uint16_t FileClusterLow;
    uint32_t FileSize;
} Fat32DirectoryEntry;

typedef struct __attribute__((packed)) {
    uint8_t Order;
    uint16_t First5[5];
    uint8_t Attributes;
    uint8_t LongEntryType;
    uint8_t Checksum;
    uint16_t Next6[6];
    uint16_t AlwaysZero;
    uint16_t Final2[2];
} Fat32LongFileNameEntry;

#endif /* _FAT32_H_ */
