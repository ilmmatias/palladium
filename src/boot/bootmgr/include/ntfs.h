/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _NTFS_H_
#define _NTFS_H_

#include <stdint.h>

typedef struct __attribute__((packed)) {
    char JumpBoot[3];
    char FileSystemName[8];
    uint16_t BytesPerSector;
    uint8_t SectorsPerCluster;
    uint16_t ReservedSectors;
    uint8_t NumberOfFats;
    uint16_t RootEntries;
    uint16_t NumberOfSectors16;
    uint8_t Media;
    uint16_t SectorsPerFat;
    uint16_t SectorsPerTrack;
    uint16_t NumberOfHeads;
    uint32_t HiddenSectors;
    uint32_t NumberOfSectors32;
    uint8_t DiscNumber;
    uint8_t Flags;
    uint8_t BpbSignature;
    char Reserved;
    uint64_t NumberOfSectors;
    uint64_t MftCluster;
    uint64_t MirrorMftCluster;
    uint8_t MftEntrySize;
    char Reserved2[3];
    uint8_t IndexEntrySize;
    char Reserved3[3];
    uint64_t SerialNumber;
    uint32_t Checksum;
    char BootCode[426];
    uint16_t SectorSignaure;
} NtfsBootSector;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint16_t FixupOffset;
    uint16_t NumberOfFixups;
    uint64_t TransactionJournalNumber;
    uint16_t Sequence;
    uint16_t References;
    uint16_t AttributeOffset;
    uint16_t EntryFlags;
    uint32_t UsedEntrySize;
    uint32_t TotalEntrySize;
    uint64_t BaseRecordFile;
    uint16_t FirstFreeAttributeIdentifier;
} NtfsMftEntry;

typedef struct __attribute__((packed)) {
    uint32_t Type;
    uint32_t Size;
    uint8_t Resident;
    uint8_t NameSize;
    uint16_t NameOffset;
    uint16_t DataFlags;
    uint16_t Identifier;
} NtfsMftAttributeHeader;

typedef struct __attribute__((packed)) {
    uint32_t AttributeType;
    uint32_t CollationType;
    uint32_t IndexEntrySize;
    uint32_t IndexEntryNumber;
} NtfsIndexRootHeader;

#endif /* _NTFS_H_ */
