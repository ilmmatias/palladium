/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPI_H_
#define _ACPI_H_

#include <stdint.h>

#define ACPI_VALUE_INTEGER 0
#define ACPI_VALUE_REGION 1

#define ACPI_NAMED_FIELD 0
#define ACPI_RESERVED_FIELD 1
#define ACPI_ACCESS_FIELD 2
#define ACPI_EXT_ACCESS_FIELD 3
#define ACPI_CONNECT_FIELD 4

struct AcpiValue;

typedef struct AcpiFieldElement {
    int Type;
    union {
        struct {
            char *Name;
            uint8_t Length;
        } Named;
        struct {
            uint8_t Length;
        } Reserved;
        struct {
            uint8_t Type;
            uint8_t Attrib;
        } Access;
        struct {
            uint8_t Type;
            uint8_t Attrib;
            uint8_t Length;
        } ExtAccess;
        struct AcpiValue *Connect;
    };
} AcpiFieldElement;

typedef struct AcpiValue {
    int Type;
    union {
        uint64_t Integer;
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

int AcpipReadFieldList(AcpipState *State, uint8_t *FieldFlags);

AcpiValue *AcpipExecuteTermList(AcpipState *State);
AcpiValue *AcpipExecuteTermArg(AcpipState *State);

#endif /* _ACPI_H_ */
