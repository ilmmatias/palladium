/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPIP_H_
#define _ACPIP_H_

#include <acpi.h>

typedef struct AcpipState {
    char *Scope;
    uint8_t ScopeSegs;
    const uint8_t *Code;
    uint32_t Length;
    uint32_t RemainingLength;
    struct AcpipState *Parent;
} AcpipState;

void AcpipInitializeFromRsdt(void);
void AcpipInitializeFromXsdt(void);

void AcpipPopulatePredefined(void);
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
int AcpipCreateObject(char *Name, AcpiValue *Value);

AcpipState *AcpipEnterIf(AcpipState *State, uint32_t Length);
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

void AcpipFreeFieldList(AcpiFieldElement *Root);
int AcpipReadFieldList(
    AcpipState *State,
    uint32_t Start,
    uint32_t Length,
    uint8_t *FieldFlags,
    AcpiFieldElement **Fields);

AcpiValue *AcpipExecuteTermList(AcpipState *State);
AcpiValue *AcpipExecuteTermArg(AcpipState *State);

#endif /* _ACPIP_H_ */
