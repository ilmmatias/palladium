/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <stdatomic.h>
#include <stdint.h>

#define ACPI_REVISION 0x0000000000000000

#define ACPI_EMPTY 0
#define ACPI_INTEGER 1
#define ACPI_STRING 2
#define ACPI_BUFFER 3
#define ACPI_PACKAGE 4
#define ACPI_FIELD_UNIT 5
#define ACPI_DEVICE 6
#define ACPI_EVENT 7
#define ACPI_METHOD 8
#define ACPI_MUTEX 9
#define ACPI_REGION 10
#define ACPI_POWER 11
#define ACPI_PROCESSOR 12
#define ACPI_THERMAL 13
#define ACPI_BUFFER_FIELD 14
#define ACPI_DEBUG 16
#define ACPI_ALIAS 17
#define ACPI_SCOPE 18
#define ACPI_REFERENCE 19
#define ACPI_INDEX 20
#define ACPI_LOCAL 21
#define ACPI_ARG 22

#define ACPI_FIELD 0
#define ACPI_BANK_FIELD 1
#define ACPI_INDEX_FIELD 2

#define ACPI_SPACE_SYSTEM_MEMORY 0
#define ACPI_SPACE_SYSTEM_IO 1
#define ACPI_SPACE_PCI_CONFIG 2
#define ACPI_SPACE_EMBEDDED_CONTROL 3
#define ACPI_SPACE_SMBUS 4
#define ACPI_SPACE_SYSTEM_CMOS 5
#define ACPI_SPACE_PCI_BAR_TARGET 6
#define ACPI_SPACE_IPMI 7
#define ACPI_SPACE_GENERAL_PURPOSE_IO 8
#define ACPI_SPACE_GENERIC_SERIAL_BUS 9
#define ACPI_SPACE_PCC 10

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

AcpiObject *AcpiSearchObject(AcpiObject *Parent, const char *Name);
char *AcpiGetObjectPath(AcpiObject *Object);
int AcpiEvaluateObject(AcpiObject *Object, AcpiValue *Result, int ExpectedType);
int AcpiExecuteMethod(AcpiObject *Object, int ArgCount, AcpiValue *Arguments, AcpiValue *Result);

void AcpiCreateReference(AcpiValue *Source, AcpiValue *Target);
void AcpiRemoveReference(AcpiValue *Value, int CleanupPointer);

int AcpiCopyValue(AcpiValue *Source, AcpiValue *Target);

#endif /* _ACPI_H_ */
