/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPIP_H_
#define _ACPIP_H_

#include <acpi.h>

#define ACPI_TARGET_LOCAL 0

typedef struct {
    int Type;
    union {
        int LocalIndex;
    };
} AcpipTarget;

typedef struct {
    AcpiObject *LinkedObject;
    const uint8_t *Start;
    int BacktrackCount;
    int SegmentCount;
} AcpipName;

typedef struct AcpipScope {
    AcpiObject *LinkedObject;
    const uint8_t *Predicate;
    uint32_t PredicateBacktrack;
    const uint8_t *Code;
    uint32_t Length;
    uint32_t RemainingLength;
    struct AcpipScope *Parent;
} AcpipScope;

typedef struct AcpipState {
    int IsMethod;
    AcpiValue Arguments[7];
    AcpiValue Locals[7];
    AcpipScope *Scope;
} AcpipState;

void AcpipInitializeFromRsdt(void);
void AcpipInitializeFromXsdt(void);

void AcpipPopulatePredefined(void);
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpiObject *AcpipCreateObject(AcpipName *Name, AcpiValue *Value);
AcpiObject *AcpipResolveObject(AcpipName *Name);

AcpipScope *AcpipEnterScope(AcpipState *State, AcpiObject *Object, uint32_t Length);
AcpipScope *AcpipEnterIf(AcpipState *State, uint32_t Length);
AcpipScope *AcpipEnterWhile(
    AcpipState *State,
    const uint8_t *Predicate,
    uint32_t PredicateBacktrack,
    uint32_t Length);

int AcpipReadByte(AcpipState *State, uint8_t *Byte);
int AcpipReadWord(AcpipState *State, uint16_t *Word);
int AcpipReadDWord(AcpipState *State, uint32_t *DWord);
int AcpipReadQWord(AcpipState *State, uint64_t *QWord);
int AcpipReadPkgLength(AcpipState *State, uint32_t *Length);
AcpipName *AcpipReadName(AcpipState *State);

int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Value);
int AcpipExecuteConcatOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteConvOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteDataObjOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteFieldOpcode(AcpipState *State, uint16_t Opcode);
int AcpipExecuteMathOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteNamedObjOpcode(AcpipState *State, uint16_t Opcode);
int AcpipExecuteNsModOpcode(AcpipState *State, uint16_t Opcode);
int AcpipExecuteStmtOpcode(AcpipState *State, uint16_t Opcode);

int AcpipCastToInteger(AcpiValue *Value, uint64_t *Result);
int AcpipCastToString(AcpiValue *Value, int ImplicitCast, int Decimal);
int AcpipCastToBuffer(AcpiValue *Value);

int AcpipExecuteInteger(AcpipState *State, uint64_t *Result);

AcpiValue *AcpipExecuteTermList(AcpipState *State);
AcpipTarget *AcpipExecuteSuperName(AcpipState *State);
AcpipTarget *AcpipExecuteTarget(AcpipState *State);
AcpiValue *AcpipReadTarget(AcpipState *State, AcpipTarget *Target);
void AcpipStoreTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value);

#endif /* _ACPIP_H_ */
