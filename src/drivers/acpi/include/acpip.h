/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPIP_H_
#define _ACPIP_H_

#include <acpi.h>
#include <sdt.h>

#define ACPI_TARGET_NONE 0
#define ACPI_TARGET_LOCAL 1
#define ACPI_TARGET_ARG 2
#define ACPI_TARGET_NAMED 3
#define ACPI_TARGET_INDEX 4
#define ACPI_TARGET_UNRESOLVED 4

#define ACPI_REASON_OUT_OF_MEMORY 0
#define ACPI_REASON_CORRUPTED_TABLES 1

typedef struct {
    int Type;
    int Index;
    union {
        AcpiObject *Object;
        AcpiValue *Source;
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
    int HasReturned;
    AcpiValue Arguments[7];
    AcpiValue Locals[8];
    AcpiValue ReturnValue;
    AcpipScope *Scope;
} AcpipState;

SdtHeader *AcpipFindTable(char Signature[4], int Index);
void AcpipReadTables(void);

void AcpipPopulatePredefined(void);
void AcpipPopulateOverride(void);
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpiObject *AcpipCreateObject(AcpipName *Name, AcpiValue *Value);
AcpiObject *AcpipResolveObject(AcpipName *Name);

void AcpipShowDebugMessage(const char *Format, ...);
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Format, ...);

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
int AcpipReadName(AcpipState *State, AcpipName *Name);

int AcpipReadField(AcpiValue *Source, AcpiValue *Target);
int AcpipWriteField(AcpiValue *Target, AcpiValue *Value);

uint64_t AcpipReadMmioSpace(uint64_t Address, int Size);
void AcpipWriteMmioSpace(uint64_t Address, int Size, uint64_t Data);
uint64_t AcpipReadPciConfigSpace(AcpiValue *Source, int Offset, int Size);
void AcpipWritePciConfigSpace(AcpiValue *Source, int Offset, int Size, uint64_t Data);
uint64_t AcpipReadIoSpace(int Offset, int Size);
void AcpipWriteIoSpace(int Offset, int Size, uint64_t Data);

int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Value, int ObjReference);
int AcpipExecuteConcatOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteConvOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteDataObjOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteFieldOpcode(AcpipState *State, uint16_t Opcode);
int AcpipExecuteLockOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteMathOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteNamedObjOpcode(AcpipState *State, uint16_t Opcode);
int AcpipExecuteNsModOpcode(AcpipState *State, uint16_t Opcode);
int AcpipExecuteRefOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value);
int AcpipExecuteStmtOpcode(AcpipState *State, uint16_t Opcode);

int AcpipCastToInteger(AcpiValue *Value, uint64_t *Result, int Cleanup);
int AcpipCastToString(AcpiValue *Value, int ImplicitCast, int Decimal);
int AcpipCastToBuffer(AcpiValue *Value);

int AcpipExecuteInteger(AcpipState *State, uint64_t *Result);
int AcpipExecuteBuffer(AcpipState *State, AcpiValue *Result);

int AcpipExecuteTermList(AcpipState *State);
int AcpipExecuteSimpleName(AcpipState *State, AcpipTarget *Target);
int AcpipExecuteSuperName(AcpipState *State, AcpipTarget *Target, int Optional);
int AcpipExecuteTarget(AcpipState *State, AcpipTarget *Target);
int AcpipReadTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value);
int AcpipStoreTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value);

#endif /* _ACPIP_H_ */
