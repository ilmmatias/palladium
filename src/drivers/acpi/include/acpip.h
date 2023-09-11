/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPIP_H_
#define _ACPIP_H_

#include <acpi.h>

typedef struct {
    AcpiObject *LinkedObject;
    const uint8_t *Start;
    int BacktrackCount;
    int SegmentCount;
} AcpipName;

typedef struct AcpipScope {
    AcpiObject *LinkedObject;
    const uint8_t *Code;
    uint32_t Length;
    uint32_t RemainingLength;
    struct AcpipScope *Parent;
} AcpipScope;

typedef struct AcpipState {
    int IsMethod;
    AcpiObject Arguments[7];
    AcpipScope *Scope;
} AcpipState;

void AcpipInitializeFromRsdt(void);
void AcpipInitializeFromXsdt(void);

void AcpipPopulatePredefined(void);
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpiObject *AcpipCreateObject(AcpipName *Name, AcpiValue *Value);
AcpiObject *AcpipResolveObject(AcpipName *Name);

AcpipScope *AcpipEnterIf(AcpipState *State, uint32_t Length);
AcpipScope *AcpipEnterScope(AcpipState *State, AcpiObject *Object, uint32_t Length);

int AcpipReadByte(AcpipState *State, uint8_t *Byte);
int AcpipReadWord(AcpipState *State, uint16_t *Word);
int AcpipReadDWord(AcpipState *State, uint32_t *DWord);
int AcpipReadQWord(AcpipState *State, uint64_t *QWord);
int AcpipReadPkgLength(AcpipState *State, uint32_t *Length);
AcpipName *AcpipReadName(AcpipState *State);

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
