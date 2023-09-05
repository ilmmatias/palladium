/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <stdint.h>

#define ACPI_INTEGER 0
#define ACPI_STRING 1
#define ACPI_BUFFER 2
#define ACPI_PACKAGE 3
#define ACPI_METHOD 4
#define ACPI_MUTEX 5
#define ACPI_REGION 6
#define ACPI_SCOPE 7
#define ACPI_DEVICE 8
#define ACPI_PROCESSOR 9

#define ACPI_NAMED_FIELD 0
#define ACPI_RESERVED_FIELD 1
#define ACPI_ACCESS_FIELD 2
#define ACPI_CONNECT_FIELD 3

struct AcpiValue;

typedef struct AcpiObject {
    char *Name;
    struct AcpiValue *Value;
    struct AcpiObject *Next;
} AcpiObject;

typedef struct AcpiFieldElement {
    int Type;
    union {
        struct {
            char Name[4];
            uint32_t Length;
        } Named;
        struct {
            uint32_t Length;
        } Reserved;
        struct {
            uint8_t Type;
            uint8_t Attrib;
            uint8_t Length;
        } Access;
    };
    struct AcpiFieldElement *Next;
} AcpiFieldElement;

typedef struct {
    int Type;
    union {
        char *String;
        struct AcpiValue *Value;
    };
} AcpiPackageElement;

typedef struct AcpiValue {
    int Type;
    AcpiObject *Objects;
    union {
        uint64_t Integer;
        char *String;
        struct {
            uint64_t Size;
            uint8_t *Data;
        } Buffer;
        struct {
            uint8_t Size;
            AcpiPackageElement *Data;
        } Package;
        struct {
            const uint8_t *Start;
            uint32_t Size;
            uint8_t Flags;
        } Method;
        struct {
            uint8_t Flags;
        } Mutex;
        struct {
            uint8_t RegionSpace;
            uint64_t RegionOffset;
            uint64_t RegionLen;
            int HasField;
            uint8_t FieldFlags;
            AcpiFieldElement *FieldList;
        } Region;
        struct {
            uint8_t ProcId;
            uint32_t PblkAddr;
            uint8_t PblkLen;
        } Processor;
    };
} AcpiValue;

AcpiObject *AcpiSearchObject(const char *Name);

#endif /* _ACPI_H_ */
