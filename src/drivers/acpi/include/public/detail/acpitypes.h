/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _ACPI_DETAIL_ACPITYPES_H_
#define _ACPI_DETAIL_ACPITYPES_H_

#include <stdatomic.h>
#include <stdint.h>

struct AcpiPackage;
struct AcpiValue;
struct AcpiObject;

typedef int (
    *AcpiOverrideMethod)(int ArgCount, struct AcpiValue *Arguments, struct AcpiValue *Result);

typedef struct {
    struct AcpiObject *LinkedObject;
    const uint8_t *Start;
    int BacktrackCount;
    int SegmentCount;
} AcpiName;

typedef struct {
    int References;
    char Data[];
} AcpiString;

typedef struct {
    int References;
    uint64_t Size;
    uint8_t Data[];
} AcpiBuffer;

typedef struct {
    int References;
    uint8_t Flags;
    volatile atomic_flag Value;
} AcpiMutex;

typedef struct {
    int References;
    struct AcpiObject *Objects;
} AcpiChildren;

typedef struct AcpiValue {
    int Type;
    int References;
    AcpiChildren *Children;
    union {
        struct AcpiObject *Alias;
        struct AcpiObject *Reference;
        uint64_t Integer;
        AcpiString *String;
        AcpiBuffer *Buffer;
        struct AcpiPackage *Package;
        AcpiMutex *Mutex;
        struct {
            AcpiOverrideMethod Override;
            const uint8_t *Start;
            uint32_t Size;
            uint8_t Flags;
        } Method;
        struct {
            uint8_t RegionSpace;
            uint64_t RegionOffset;
            uint64_t RegionLen;
            int PciReady;
            uint32_t PciDevice;
            uint32_t PciFunction;
            uint32_t PciSegment;
            uint32_t PciBus;
        } Region;
        struct {
            int FieldType;
            struct AcpiObject *Region;
            struct AcpiObject *Data;
            uint8_t AccessType;
            uint8_t AccessAttrib;
            uint8_t AccessLength;
            uint32_t Offset;
            uint32_t Length;
            uint64_t BankValue;
        } FieldUnit;
        struct {
            struct AcpiValue *Source;
            uint64_t Index;
            int Size;
        } BufferField;
        struct {
            uint8_t ProcId;
            uint32_t PblkAddr;
            uint8_t PblkLen;
        } Processor;
        struct {
            uint8_t SystemLevel;
            uint16_t ResourceOrder;
        } Power;
    };
} AcpiValue;

typedef struct {
    int Type;
    union {
        AcpiName Name;
        AcpiValue Value;
    };
} AcpiPackageElement;

typedef struct AcpiPackage {
    int References;
    uint64_t Size;
    AcpiPackageElement Data[];
} AcpiPackage;

typedef struct AcpiObject {
    char Name[4];
    struct AcpiValue Value;
    struct AcpiObject *Next;
    struct AcpiObject *Parent;
} AcpiObject;

#endif /* _ACPI_DETAIL_ACPITYPES_H_ */
