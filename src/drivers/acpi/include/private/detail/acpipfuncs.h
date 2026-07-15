/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/* IWYU pragma: private, include <acpip.h> */

#ifndef _ACPIP_DETAIL_ACPIPFUNCS_H_
#define _ACPIP_DETAIL_ACPIPFUNCS_H_

#include <detail/acpiptypes.h>
#include <sdt.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

SdtHeader *AcpipFindTable(char Signature[4], int Index);
void AcpipReadTables(void);

void AcpipPopulatePredefined(void);
void AcpipPopulateOverride(void);
void AcpipPopulateTree(const uint8_t *Code, uint32_t Length);
AcpiObject *AcpipCreateObject(AcpiName *Name, AcpiValue *Value);
AcpiObject *AcpipResolveObject(AcpiName *Name);

void AcpiCreateReference(AcpiValue *Source, AcpiValue *Target);
void AcpiRemoveReference(AcpiValue *Value, bool CleanupPointer);

void *AcpipAllocateBlock(size_t Size);
void *AcpipAllocateZeroBlock(size_t Elements, size_t ElementSize);
void AcpipFreeBlock(void *Block);

void AcpipShowInfoMessage(const char *Message, ...);
void AcpipShowDebugMessage(const char *Message, ...);
void AcpipShowTraceMessage(const char *Message, ...);
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Message, ...);

uint64_t AcpipGetTimer(void);
void AcpipDelayThread(uint64_t Milliseconds);
void AcpipStallExecution(uint64_t Microseconds);

void *AcpipCreateEvent(void);
void AcpipDeleteEvent(void *Event);
bool AcpipWaitEvent(void *Event, uint64_t Timeout);
void AcpipSignalEvent(void *Event);
void AcpipResetEvent(void *Event);

void *AcpipCreateMutex(void);
void AcpipDeleteMutex(void *Mutex);
bool AcpipAcquireMutex(void *Mutex, uint64_t Timeout);
void AcpipReleaseMutex(void *Mutex);

void AcpipInitializeGlobalLock(void);
bool AcpipAcquireGlobalLock(void);
bool AcpipReleaseGlobalLock(void);

uint64_t AcpipReadEcSpace(uint16_t CmdPort, uint16_t DataPort, uint64_t Offset);
void AcpipWriteEcSpace(uint16_t CmdPort, uint16_t DataPort, uint64_t Offset, uint64_t Data);

uint64_t AcpipReadCmosSpace(uint64_t Offset);
void AcpipWriteCmosSpace(uint64_t Offset, uint64_t Data);

AcpipScope *AcpipEnterScope(AcpipState *State, AcpiObject *Object, uint32_t Length);
AcpipScope *AcpipEnterIf(AcpipState *State, uint32_t Length);
AcpipScope *AcpipEnterWhile(
    AcpipState *State,
    const uint8_t *Predicate,
    uint32_t PredicateBacktrack,
    uint32_t Length);

bool AcpipReadByte(AcpipState *State, uint8_t *Byte);
bool AcpipReadWord(AcpipState *State, uint16_t *Word);
bool AcpipReadDWord(AcpipState *State, uint32_t *DWord);
bool AcpipReadQWord(AcpipState *State, uint64_t *QWord);
bool AcpipReadPkgLength(AcpipState *State, uint32_t *Length);
bool AcpipReadName(AcpipState *State, AcpiName *Name);

bool AcpipReadField(AcpiValue *Source, AcpiValue *Target);
bool AcpipWriteField(AcpiValue *Target, AcpiValue *Data);

uint64_t AcpipReadMmioSpace(uint64_t Address, uint64_t Size);
void AcpipWriteMmioSpace(uint64_t Address, uint64_t Size, uint64_t Data);
uint64_t AcpipReadPciConfigSpace(AcpiValue *Source, uint64_t Offset, uint64_t Size);
void AcpipWritePciConfigSpace(AcpiValue *Source, uint64_t Offset, uint64_t Size, uint64_t Data);
uint64_t AcpipReadIoSpace(uint64_t Offset, uint64_t Size);
void AcpipWriteIoSpace(uint64_t Offset, uint64_t Size, uint64_t Data);

bool AcpipPrepareExecuteOpcode(AcpipState *State);
bool AcpipExecuteOpcode(AcpipState *State, AcpiValue *Result);
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

bool AcpipCastToInteger(AcpiValue *Value, uint64_t *Result, bool Cleanup);
bool AcpipCastToString(AcpiValue *Value, bool ImplicitCast, bool Decimal);
bool AcpipCastToBuffer(AcpiValue *Value);

bool AcpipExecuteTermList(AcpipState *State);
bool AcpipReadTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value);
bool AcpipStoreTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _ACPIP_DETAIL_ACPIPFUNCS_H_ */
