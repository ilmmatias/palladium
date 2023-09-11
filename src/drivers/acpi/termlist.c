/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AcpiValue *AcpipExecuteTermList(AcpipState *State) {
    while (1) {
        if (!State->Scope->RemainingLength) {
            /* Backtrack into the previous scope, or end if we're already in the top-most
               scope. */
            if (!State->Scope->Parent) {
                break;
            }

            AcpipScope *Parent = State->Scope->Parent;
            Parent->Code = State->Scope->Code;
            Parent->RemainingLength -= State->Scope->Length;

            free(State->Scope);
            State->Scope = Parent;

            continue;
        }

        uint8_t Opcode = *(State->Scope->Code++);
        State->Scope->RemainingLength--;

        uint8_t ExtOpcode = 0;
        if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
            return NULL;
        }

        uint32_t Start = State->Scope->RemainingLength;
        const uint8_t *StartCode = State->Scope->Code;

        switch (Opcode | ((uint16_t)ExtOpcode << 8)) {
            /* DefAlias := AliasOp NameString NameString */
            case 0x06: {
                AcpipName *SourceName = AcpipReadName(State);
                if (!SourceName) {
                    return NULL;
                }

                AcpipName *AliasName = AcpipReadName(State);
                if (!AliasName) {
                    free(SourceName);
                    return NULL;
                }

                AcpiObject *SourceObject = AcpipResolveObject(SourceName);
                if (!SourceObject) {
                    free(AliasName);
                    free(SourceName);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_ALIAS;
                Value.Alias = SourceObject;

                AcpiObject *Object = AcpipCreateObject(AliasName, &Value);
                if (!Object) {
                    free(AliasName);
                    return NULL;
                }

                break;
            }

            /* DefName := NameOp NameString DataRefObject */
            case 0x08: {
                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                AcpiValue *DataRefObject = AcpipExecuteTermArg(State);
                if (!DataRefObject) {
                    free(Name);
                    return NULL;
                }

                AcpiObject *Object = AcpipCreateObject(Name, DataRefObject);
                if (!Object) {
                    free(DataRefObject);
                    free(Name);
                    return NULL;
                }

                free(DataRefObject);
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

                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    free(Name);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = Opcode == 0x10 ? ACPI_SCOPE : ACPI_DEVICE;
                Value.Objects = NULL;

                AcpiObject *Object = AcpipCreateObject(Name, &Value);
                if (!Object) {
                    free(Name);
                    return NULL;
                }

                AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
                if (!Scope) {
                    return NULL;
                }

                State->Scope = Scope;
                break;
            }

            /* DefMethod := MethodOp PkgLength NameString MethodFlags TermList */
            case 0x14: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar >= Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    free(Name);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_METHOD;
                Value.Method.Start = State->Scope->Code + 1;
                Value.Method.Size = Length - LengthSoFar;
                Value.Method.Flags = *State->Scope->Code;

                if (!AcpipCreateObject(Name, &Value)) {
                    free(Name);
                    return NULL;
                }

                State->Scope->Code += Length - LengthSoFar;
                State->Scope->RemainingLength -= Length - LengthSoFar;
                break;
            }

            /* DefMutex := MutexOp NameString SyncFlags */
            case 0x015B: {
                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                uint8_t SyncFlags;
                if (!AcpipReadByte(State, &SyncFlags)) {
                    free(Name);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_MUTEX;
                Value.Mutex.Flags = SyncFlags;

                if (!AcpipCreateObject(Name, &Value)) {
                    free(Name);
                    return NULL;
                }

                break;
            }

            /* DefEvent := EventOp NameString */
            case 0x025B: {
                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_EVENT;

                if (!AcpipCreateObject(Name, &Value)) {
                    free(Name);
                    return NULL;
                }

                break;
            }

            /* DefIfElse := IfOp PkgLength Predicate TermList DefElse */
            case 0xA0: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                AcpiValue *Predicate = AcpipExecuteTermArg(State);
                if (!Predicate) {
                    return NULL;
                } else if (Predicate->Type != ACPI_INTEGER) {
                    free(Predicate);
                    return NULL;
                }

                uint64_t PredicateValue = Predicate->Integer;
                free(Predicate);

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    return NULL;
                }

                if (PredicateValue) {
                    AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
                    if (!Scope) {
                        return NULL;
                    }

                    State->Scope = Scope;
                    break;
                }

                State->Scope->Code += Length - LengthSoFar;
                State->Scope->RemainingLength -= Length - LengthSoFar;

                /* DefElse only really matter for If(false), and we ignore it anywhere else;
                   Here, we try reading up the code (and entering the else scope) after
                   If(false). */
                if (!State->Scope->RemainingLength || *State->Scope->Code != 0xA1) {
                    break;
                }

                State->Scope->Code++;
                State->Scope->RemainingLength--;
                Start = State->Scope->RemainingLength;

                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                LengthSoFar = Start - State->Scope->RemainingLength;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    return NULL;
                }

                AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
                if (!Scope) {
                    return NULL;
                }

                State->Scope = Scope;
                break;
            }

            /* DefElse := ElseOp PkgLength TermList */
            case 0xA1: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length) || Length > Start) {
                    return NULL;
                }

                State->Scope->Code = StartCode + Length;
                State->Scope->RemainingLength = Start - Length;
                break;
            }

            /* DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen */
            case 0x805B: {
                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
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

                AcpiValue Value;
                Value.Type = ACPI_REGION;
                Value.Objects = NULL;
                Value.Region.RegionSpace = RegionSpace;
                Value.Region.RegionLen = RegionLen->Integer;
                Value.Region.RegionOffset = RegionOffset->Integer;

                free(RegionLen);
                free(RegionOffset);
                if (!AcpipCreateObject(Name, &Value)) {
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

                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                AcpiObject *Object = AcpipResolveObject(Name);
                if (!Object) {
                    free(Name);
                    return NULL;
                } else if (Object->Value.Type != ACPI_REGION) {
                    return NULL;
                }

                AcpiValue Base;
                Base.Type = ACPI_FIELD;
                Base.Field.Region = Object;
                if (!AcpipReadFieldList(State, &Base, Start, Length)) {
                    return NULL;
                }

                break;
            }

            /* DefProcessor := ProcessorOp PkgLength NameString ProcID PblkAddr PblkLen TermList */
            case 0x835B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength + 6;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    free(Name);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_PROCESSOR;
                Value.Processor.ProcId = *(State->Scope->Code);
                Value.Processor.PblkAddr = *(uint32_t *)(State->Scope->Code + 1);
                Value.Processor.PblkLen = *(State->Scope->Code + 5);

                State->Scope->Code += 6;
                State->Scope->RemainingLength -= 6;

                AcpiObject *Object = AcpipCreateObject(Name, &Value);
                if (!Object) {
                    free(Name);
                    return NULL;
                }

                AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
                if (!Scope) {
                    return NULL;
                }

                State->Scope = Scope;
                break;
            }

            /* DefPowerRes := PowerResOp PkgLength NameString SystemLevel ResourceOrder TermList */
            case 0x845B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                AcpipName *Name = AcpipReadName(State);
                if (!Name) {
                    return NULL;
                }

                uint32_t LengthSoFar = Start - State->Scope->RemainingLength + 3;
                if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                    free(Name);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_POWER;
                Value.Power.SystemLevel = *(State->Scope->Code);
                Value.Power.ResourceOrder = *(uint16_t *)(State->Scope->Code + 1);

                State->Scope->Code += 3;
                State->Scope->RemainingLength -= 3;

                AcpiObject *Object = AcpipCreateObject(Name, &Value);
                if (!Object) {
                    free(Name);
                    return NULL;
                }

                AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
                if (!Scope) {
                    return NULL;
                }

                State->Scope = Scope;
                break;
            }

            /* DefIndexField := IndexFieldOp PkgLength NameString NameString FieldFlags FieldList */
            case 0x865B: {
                uint32_t Length;
                if (!AcpipReadPkgLength(State, &Length)) {
                    return NULL;
                }

                AcpipName *Index = AcpipReadName(State);
                if (!Index) {
                    return NULL;
                }

                AcpipName *Data = AcpipReadName(State);
                if (!Data) {
                    free(Index);
                    return NULL;
                }

                printf("Next 16 characters of Index: %.16s\n", Index->Start);
                printf("Next 16 characters of Data: %.16s\n", Data->Start);

                while (1)
                    ;
            }

            default:
                printf(
                    "unimplemented termlist opcode: %#hx; %d bytes left to parse out of %d.\n",
                    Opcode | ((uint32_t)ExtOpcode << 8),
                    State->Scope->RemainingLength,
                    State->Scope->Length);
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
