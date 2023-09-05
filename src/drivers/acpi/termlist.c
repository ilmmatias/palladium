/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AcpiValue *AcpipExecuteTermList(AcpipState *State) {
    while (1) {
        if (!State->RemainingLength) {
            /* Backtrack into the previous scope, or end if we're already in the top-most
               scope. */
            if (State->Parent) {
                AcpipState *Parent = State->Parent;

                if (!State->InMethod) {
                    Parent->Code = State->Code;
                    Parent->RemainingLength -= State->Length;
                }

                free(State->Scope);
                free(State);
                State = Parent;
                continue;
            }

            break;
        }

        uint8_t Opcode = *(State->Code++);
        State->RemainingLength--;

        uint8_t ExtOpcode = 0;
        if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
            return NULL;
        }

        uint32_t Start = State->RemainingLength;
        switch (Opcode | ((uint16_t)ExtOpcode << 8)) {
            /* DefName := NameOp NameString DataRefObject */
            case 0x08: {
                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                AcpiValue *DataRefObject = AcpipExecuteTermArg(State);
                if (!DataRefObject) {
                    free(Name);
                    return NULL;
                } else if (!AcpipCreateObject(Name, DataRefObject)) {
                    free(DataRefObject);
                    free(Name);
                    return NULL;
                }

                break;
            }

            /* DefScope := ScopeOp PkgLength NameString TermList
             * DefDevice := DeviceOp PkgLength NameString TermList */
            case 0x10:
            case 0x825B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                char *ObjectName = strdup(Name);
                if (!ObjectName) {
                    free(Name);
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->RemainingLength) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                AcpipState *Scope = AcpipEnterSubScope(State, Name, NameSegs, Length - LengthSoFar);
                if (!Scope) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                AcpiValue *Value = malloc(sizeof(AcpiValue));
                if (!Value) {
                    free(Scope);
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                Value->Type = Opcode == 0x10 ? ACPI_SCOPE : ACPI_DEVICE;
                Value->Objects = NULL;

                if (!AcpipCreateObject(ObjectName, Value)) {
                    free(Value);
                    free(Scope);
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                State = Scope;
                break;
            }

            /* DefMethod := MethodOp PkgLength NameString MethodFlags TermList */
            case 0x14: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                uint8_t MethodFlags;
                if (!AcpipReadByte(State, &MethodFlags)) {
                    free(Name);
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->RemainingLength) {
                    free(Name);
                    return NULL;
                }

                AcpiValue *Value = malloc(sizeof(AcpiValue));
                if (!Value) {
                    free(Name);
                    return NULL;
                }

                Value->Type = ACPI_METHOD;
                Value->Method.Start = State->Code;
                Value->Method.Size = Length - LengthSoFar;
                Value->Method.Flags = MethodFlags;
                if (!AcpipCreateObject(Name, Value)) {
                    free(Value);
                    free(Name);
                    return NULL;
                }

                State->Code += Length - LengthSoFar;
                State->RemainingLength -= Length - LengthSoFar;
                break;
            }

            /* DefMutex := MutexOp NameString SyncFlags */
            case 0x015B: {
                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                uint8_t SyncFlags;
                if (!AcpipReadByte(State, &SyncFlags)) {
                    free(Name);
                    return NULL;
                }

                AcpiValue *Value = malloc(sizeof(AcpiValue));
                if (!Value) {
                    free(Name);
                    return NULL;
                }

                Value->Type = ACPI_MUTEX;
                Value->Mutex.Flags = SyncFlags;
                if (!AcpipCreateObject(Name, Value)) {
                    free(Value);
                    free(Name);
                    return NULL;
                }

                break;
            }

            /* DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen */
            case 0x805B: {
                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                uint8_t RegionSpace;
                if (!AcpipReadByte(State, &RegionSpace)) {
                    free(Name);
                    return NULL;
                }

                /* RegionSpace is already a guaranteed uint8_t, but the other values have to be
                   coerced into integers; If they aren't integers, we have something wrong with
                   the AML code. */
                AcpiValue *RegionOffset = AcpipExecuteTermArg(State);
                if (!RegionOffset) {
                    free(Name);
                    return NULL;
                } else if (RegionOffset->Type != ACPI_INTEGER) {
                    free(RegionOffset);
                    free(Name);
                    return NULL;
                }

                AcpiValue *RegionLen = AcpipExecuteTermArg(State);
                if (!RegionLen) {
                    free(RegionOffset);
                    free(Name);
                    return NULL;
                } else if (RegionLen->Type != ACPI_INTEGER) {
                    free(RegionLen);
                    free(RegionOffset);
                    free(Name);
                    return NULL;
                }

                AcpiValue *Value = malloc(sizeof(AcpiValue));
                if (!Value) {
                    free(RegionLen);
                    free(RegionOffset);
                    free(Name);
                    return NULL;
                }

                Value->Type = ACPI_REGION;
                Value->Region.RegionSpace = RegionSpace;
                Value->Region.RegionLen = RegionLen->Integer;
                Value->Region.RegionOffset = RegionOffset->Integer;
                Value->Region.HasField = 0;

                free(RegionLen);
                free(RegionOffset);
                if (!AcpipCreateObject(Name, Value)) {
                    free(Name);
                    return NULL;
                }

                break;
            }

            /* DefField := FieldOp PkgLength NameString FieldFlags FieldList */
            case 0x815B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                uint8_t FieldFlags;
                AcpiFieldElement *Fields = NULL;
                if (!AcpipReadFieldList(State, Start, Length, &FieldFlags, &Fields)) {
                    free(Name);
                    return NULL;
                }

                AcpiObject *Object = AcpiSearchObject(Name);
                free(Name);

                if (!Object || Object->Value->Type != ACPI_REGION) {
                    AcpipFreeFieldList(Fields);
                    return NULL;
                } else if (Object->Value->Region.HasField) {
                    AcpipFreeFieldList(Object->Value->Region.FieldList);
                }

                Object->Value->Region.HasField = 1;
                Object->Value->Region.FieldFlags = FieldFlags;
                Object->Value->Region.FieldList = Fields;
                break;
            }

            /* DefProcessor := ProcessorOp PkgLength NameString ProcID PblkAddr PblkLen TermList */
            case 0x835B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                char *Name;
                uint8_t NameSegs;
                if (!AcpipReadNameString(State, &Name, &NameSegs)) {
                    return NULL;
                }

                char *ObjectName = strdup(Name);
                if (!ObjectName) {
                    free(Name);
                    return NULL;
                }

                uint8_t ProcId;
                if (!AcpipReadByte(State, &ProcId)) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                uint32_t PblkAddr;
                if (!AcpipReadDWord(State, &PblkAddr)) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                uint8_t PblkLen;
                if (!AcpipReadByte(State, &PblkLen)) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->RemainingLength) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                AcpipState *Scope = AcpipEnterSubScope(State, Name, NameSegs, Length - LengthSoFar);
                if (!Scope) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                AcpiValue *Value = malloc(sizeof(AcpiValue));
                if (!Value) {
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                Value->Type = ACPI_PROCESSOR;
                Value->Processor.ProcId = ProcId;
                Value->Processor.PblkAddr = PblkAddr;
                Value->Processor.PblkLen = PblkLen;
                Value->Objects = NULL;

                if (!AcpipCreateObject(ObjectName, Value)) {
                    free(Value);
                    free(Scope);
                    free(ObjectName);
                    free(Name);
                    return NULL;
                }

                State = Scope;
                break;
            }

            default:
                printf(
                    "unimplemented termlist opcode: %#hx; %d bytes left to parse out of %d.\n",
                    Opcode | ((uint32_t)ExtOpcode << 8),
                    State->RemainingLength,
                    State->Length);
                while (1)
                    ;
        }
    }

    AcpiValue *Value = malloc(sizeof(AcpiValue));
    if (Value) {
        Value->Type = ACPI_INTEGER;
        Value->Integer = 0;
    }

    return Value;
}
