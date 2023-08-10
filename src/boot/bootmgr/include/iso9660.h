/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ISO9660_H_
#define _ISO9660_H_

#include <stddef.h>
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t DirectoryRecordLength;
    uint8_t ExtendedAttributeRecordLength;
    uint32_t ExtentSector;
    uint32_t ExtentSectorBE;
    uint32_t ExtentSize;
    uint32_t ExtentSizeBE;
    char RecordingDate[7];
    uint8_t FileFlags;
    uint8_t FileUnitSize;
    uint8_t InterleaveGapSize;
    uint16_t VolumeSequenceNumber;
    uint16_t VolumeSequenceNumberBE;
    uint8_t NameLength;
} Iso9660DirectoryRecord;

typedef struct __attribute__((packed)) {
    uint8_t TypeCode;
    char StandardIdentifier[5];
    uint8_t Version;
    char Reserved1;
    char SystemIdentifier[32];
    char VolumeIdentifier[32];
    char Reserved2[8];
    uint32_t VolumeSpaceSize;
    uint32_t VolumeSpaceSizeBE;
    char Reserved3[32];
    uint16_t VolumeSetSize;
    uint16_t VolumeSetSizeBE;
    uint16_t VolumeSequenceNumber;
    uint16_t VolumeSequenceNumberBE;
    uint16_t LogicalBlockSize;
    uint16_t LogicalBlockSizeBE;
    uint32_t PathTableSize;
    uint32_t PathTableSizeBE;
    uint32_t TypeLPathTable;
    uint32_t OptionalTypeLPathTable;
    uint32_t TypeMPathTable;
    uint32_t OptionalTypeMPathTable;
    Iso9660DirectoryRecord RootDirectory;
    char RootDirectoryName;
    char VolumeSetIdentifier[128];
    char PublisherIdentifier[128];
    char DataPreparerIdentifier[128];
    char ApplicationIdentifier[128];
    char CopyrightFileIdentifier[37];
    char AbstractFileIdentifier[37];
    char BibliographicFileIdentifier[37];
    char VolumeCreationDate[17];
    char VolumeModificationDate[17];
    char VolumeExpirationDate[17];
    char VolumeEffectiveDate[17];
    uint8_t FileStructureVersion;
    char Reserved4;
} Iso9660PrimaryVolumeDescriptor;

#endif /* _ISO9660_H_ */
