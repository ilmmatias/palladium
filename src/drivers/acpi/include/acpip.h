/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#ifndef _ACPIP_H_
#define _ACPIP_H_

#include <acpi.h>
#include <sdt.h>

#define ACPI_ARG_NONE 0
#define ACPI_ARG_BYTE 1
#define ACPI_ARG_WORD 2
#define ACPI_ARG_DWORD 3
#define ACPI_ARG_QWORD 4
#define ACPI_ARG_STRING 5
#define ACPI_ARG_NAME 6
#define ACPI_ARG_TERM_ARG 7
#define ACPI_ARG_OBJ_REF 8
#define ACPI_ARG_INTEGER 9
#define ACPI_ARG_BUFFER 10
#define ACPI_ARG_PACKAGE 11

#define ACPI_REASON_OUT_OF_MEMORY 0
#define ACPI_REASON_CORRUPTED_TABLES 1

typedef struct {
    int Valid;
    int Count;
    int HasPkgLength;
    int Types[6];
} AcpipArgument;

typedef union {
    uint64_t Integer;
    AcpiString *String;
    AcpiName Name;
    AcpiValue TermArg;
} AcpipArgumentValue;

typedef struct AcpipOpcode {
    const uint8_t *StartCode;
    uint32_t Start;
    const uint8_t *Predicate;
    uint32_t PredicateBacktrack;
    uint16_t Opcode;
    AcpipArgument *ArgInfo;
    int ValidPkgLength;
    uint32_t PkgLength;
    int ValidArgs;
    AcpipArgumentValue FixedArguments[6];
    AcpipArgumentValue *VariableArguments;
    int ParentArgType;
    AcpipArgumentValue *ParentArg;
    struct AcpipOpcode *Parent;
} AcpipOpcode;

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
    AcpipOpcode *Opcode;
} AcpipState;

SdtHeader *AcpipFindTable(char Signature[4], int Index);
void AcpipReadTables(void);

void AcpipPopulatePredefined(void);
void AcpipPopulateOverride(void);
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpiObject *AcpipCreateObject(AcpiName *Name, AcpiValue *Value);
AcpiObject *AcpipResolveObject(AcpiName *Name);
char *AcpipGetObjectPath(AcpiObject *Object);

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
int AcpipReadName(AcpipState *State, AcpiName *Name);

int AcpipReadField(AcpiValue *Source, AcpiValue *Target);
int AcpipWriteField(AcpiValue *Target, AcpiValue *Value);

uint64_t AcpipReadMmioSpace(uint64_t Address, int Size);
void AcpipWriteMmioSpace(uint64_t Address, int Size, uint64_t Data);
uint64_t AcpipReadPciConfigSpace(AcpiValue *Source, int Offset, int Size);
void AcpipWritePciConfigSpace(AcpiValue *Source, int Offset, int Size, uint64_t Data);
uint64_t AcpipReadIoSpace(int Offset, int Size);
void AcpipWriteIoSpace(int Offset, int Size, uint64_t Data);

int AcpipPrepareExecuteOpcode(AcpipState *State);
int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Value);
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

int AcpipExecuteTermList(AcpipState *State);
int AcpipReadTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value);
int AcpipStoreTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value);

#endif /* _ACPIP_H_ */
