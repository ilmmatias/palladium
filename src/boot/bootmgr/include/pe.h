/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _PE_H_
#define _PE_H_

#include <stdint.h>

#define PE_SIGNATURE "PE\0\0"

#if defined(ARCH_x86) || defined(ARCH_amd64)
#define PE_MACHINE 0x8664
#else
#error "Undefined ARCH for the bootmgr module!"
#endif

#define IMAGE_REL_BASED_HIGH 1
#define IMAGE_REL_BASED_LOW 2
#define IMAGE_REL_BASED_HIGHLOW 3
#define IMAGE_REL_BASED_HIGHADJ 4
#define IMAGE_REL_BASED_DIR64 10

typedef struct __attribute__((packed)) {
    uint32_t VirtualAddress;
    uint32_t Size;
} PeDataDirectory;

typedef struct __attribute__((packed)) {
    char Signature[4];
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    struct {
        PeDataDirectory ExportTable;
        PeDataDirectory ImportTable;
        PeDataDirectory ResourceTable;
        PeDataDirectory ExceptionTable;
        PeDataDirectory CertificateTable;
        PeDataDirectory BaseRelocationTable;
        PeDataDirectory Debug;
        PeDataDirectory Architecture;
        PeDataDirectory GlobalPtr;
        PeDataDirectory TlsTable;
        PeDataDirectory LoadConfigTable;
        PeDataDirectory BoundImport;
        PeDataDirectory Iat;
        PeDataDirectory DelayImportDescriptor;
        PeDataDirectory ClrRuntimeHeader;
        PeDataDirectory Reserved;
    } DataDirectories;
} PeHeader;

typedef struct __attribute__((packed)) {
    char Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} PeSectionHeader;

typedef struct __attribute__((packed)) {
    uint32_t ExportFlags;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t NameRva;
    uint32_t OrdinalBase;
    uint32_t AddressTableEntries;
    uint32_t NumberOfNamePointers;
    uint32_t ExportTableRva;
    uint32_t NamePointerRva;
    uint32_t OrdinalTableRva;
} PeExportHeader;

typedef struct __attribute__((packed)) {
    uint32_t ImportLookupTableRva;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRva;
    uint32_t ImportAddressTableRva;
} PeImportHeader;

typedef struct __attribute__((packed)) {
    uint32_t PageRva;
    uint32_t BlockSize;
} PeBaseRelocationBlock;

#endif /* _PE_H_ */
