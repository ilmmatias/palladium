/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <stdint.h>

#define ACPI_VALUE_INTEGER 0
#define ACPI_VALUE_STRING 1
#define ACPI_VALUE_BUFFER 2
#define ACPI_VALUE_REGION 3

#define ACPI_NAMED_FIELD 0
#define ACPI_RESERVED_FIELD 1
#define ACPI_ACCESS_FIELD 2
#define ACPI_CONNECT_FIELD 3

struct AcpiValue;

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

typedef struct AcpiValue {
    int Type;
    union {
        uint64_t Integer;
        char *String;
        struct {
            uint64_t Size;
            uint8_t *Data;
        } Buffer;
        struct {
            uint32_t RegionSpace;
            struct AcpiValue *RegionOffset;
            struct AcpiValue *RegionLen;
        } Region;
    };
} AcpiValue;

typedef struct {
    char *Path;
    AcpiValue Value;
} AcpiObject;

typedef struct AcpipState {
    char *Scope;
    uint8_t ScopeSegs;
    const uint8_t *Code;
    uint32_t Length;
    uint32_t RemainingLength;
    int InMethod;
    struct AcpipState *Parent;
} AcpipState;

void AcpipInitializeFromRsdt(void);
void AcpipInitializeFromXsdt(void);

void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpipState *AcpipEnterSubScope(AcpipState *State, char *Name, uint8_t NameSegs, uint32_t Length);
AcpipState *AcpipEnterMethod(
    AcpipState *State,
    char *Name,
    uint8_t NameSegs,
    const uint8_t *Code,
    uint32_t Length);

int AcpipReadByte(AcpipState *State, uint8_t *Byte);
int AcpipReadWord(AcpipState *State, uint16_t *Word);
int AcpipReadDWord(AcpipState *State, uint32_t *DWord);
int AcpipReadQWord(AcpipState *State, uint64_t *QWord);
int AcpipReadPkgLength(AcpipState *State, uint32_t *Length);
int AcpipReadNameString(AcpipState *State, char **NameString, uint8_t *NameSegs);

int AcpipReadFieldList(
    AcpipState *State,
    uint32_t Start,
    uint32_t Length,
    uint8_t *FieldFlags,
    AcpiFieldElement **Fields);

AcpiValue *AcpipExecuteTermList(AcpipState *State);
AcpiValue *AcpipExecuteTermArg(AcpipState *State);

#endif /* _ACPI_H_ */
