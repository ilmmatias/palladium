/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _OS_PE_H_
#define _OS_PE_H_

#include <stdint.h>

#define PE_SIGNATURE "PE\0\0"

#if defined(ARCH_amd64)
#define PE_MACHINE 0x8664
#else
#error "Undefined ARCH for the sdk module!"
#endif

#define PE_IMAGE_REL_BASED_ABSOLUTE 0
#define PE_IMAGE_REL_BASED_HIGH 1
#define PE_IMAGE_REL_BASED_LOW 2
#define PE_IMAGE_REL_BASED_HIGHLOW 3
#define PE_IMAGE_REL_BASED_HIGHADJ 4
#define PE_IMAGE_REL_BASED_DIR64 10

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
    uint32_t BaseOfData;
    uint32_t ImageBase;
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
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
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
} PeHeader32;

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
} PeHeader64;

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
    uint32_t Size;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t GlobalFlagsClear;
    uint32_t GlobalFlagsSet;
    uint32_t CriticalSectionDefaultTimeout;
    uintptr_t DeCommitFreeBlockThreshold;
    uintptr_t DeCommitTotalFreeThreshold;
    uintptr_t LockPrefixTable;
    uintptr_t MaximumAllocationSize;
    uintptr_t VirtualMemoryThreshold;
    uintptr_t ProcessAffinityMask;
    uint32_t ProcessHeapFlags;
    uint16_t CsdVersion;
    uint16_t DependentLoadFlags;
    uintptr_t EditList;
    uintptr_t SecurityCookie;
    uintptr_t SeHandlerTable;
    uintptr_t SeHandlerCount;
    uintptr_t GuardCfCheckFunctionPointer;
    uintptr_t GuardCfDispatchFunctionPointer;
    uintptr_t GuardCfFunctionTable;
    uintptr_t GuardCfFunctionCount;
    uint32_t GuardFlags;
    struct {
        uint16_t Flags;
        uint16_t Catalog;
        uint32_t CatalogOffset;
        uint32_t Reserved;
    } CodeIntegrity;
    uintptr_t GuardAddressTakenIatEntryTable;
    uintptr_t GuardAddressTakenIatEntryCount;
    uintptr_t GuardLongJumpTargetTable;
    uintptr_t GuardLongJumpTargetCount;
    uintptr_t DynamicValueRelocTable;
    uintptr_t ChPeMetadataPointer;
    uintptr_t GuardRfFailureRoutine;
    uintptr_t GuardRfFailureRoutineFunctionPointer;
    uint32_t DynamicValueRelocTableOffset;
    uint16_t DynamicValueRelocTableSection;
    uint16_t Reserved2;
    uintptr_t GuardRfVerifyStackPointerFunctionPointer;
    uint32_t HotPatchTableOffset;
    uint32_t Reserved3;
    uintptr_t EnclaveConfigurationPointer;
    uintptr_t VolatileMetadataPointer;
    uintptr_t GuardEhContinuationTable;
    uintptr_t GuardEhContinuationCount;
    uintptr_t GuardXfgCheckFunctionPointer;
    uintptr_t GuardXfgDispatchFunctionPointer;
    uintptr_t GuardXfgTableDispatchFunctionPointer;
    uintptr_t CastGuardOsDeterminedFailureMode;
    uintptr_t GuardMemcpyFunctionPointer;
    uintptr_t UmaFunctionPointers;
} PeLoadConfigHeader;

typedef struct __attribute__((packed)) {
    uint32_t PageRva;
    uint32_t BlockSize;
} PeBaseRelocationBlock;

typedef struct __attribute__((packed)) {
    char Name[8];
    uint32_t Value;
    uint16_t SectionNumber;
    uint16_t Type;
    uint8_t StorageClass;
    uint8_t NumberOfAuxSymbols;
} CoffSymbol;

#if UINTPTR_MAX == UINT64_MAX
typedef PeHeader64 PeHeader;
#else
typedef PeHeader32 PeHeader;
#endif

#endif /* _OS_PE_H_ */
