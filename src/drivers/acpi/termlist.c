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

            /* DefStore := StoreOp TermArg SuperName */
            case 0x70: {
                AcpiValue *Value = AcpipExecuteTermArg(State);
                if (!Value) {
                    return NULL;
                }

                AcpipTarget *Target = AcpipExecuteSuperName(State);
                if (!Target) {
                    free(Value);
                    return NULL;
                }

                AcpipStoreTarget(State, Target, Value);
                free(Target);
                free(Value);

                break;
            }

            /* DefSubtract := SubtractOp Operand Operand Target */
            case 0x74: {
                AcpiValue *Left = AcpipExecuteTermArg(State);
                if (!Left) {
                    return NULL;
                } else if (Left->Type != ACPI_INTEGER) {
                    free(Left);
                    return NULL;
                }

                AcpiValue *Right = AcpipExecuteTermArg(State);
                if (!Right) {
                    free(Left);
                    return NULL;
                } else if (Right->Type != ACPI_INTEGER) {
                    free(Right);
                    free(Left);
                    return NULL;
                }

                AcpipTarget *Target = AcpipExecuteTarget(State);
                if (!Target) {
                    free(Right);
                    free(Left);
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_INDEX_FIELD;
                Value.Integer = Left->Integer - Right->Integer;

                AcpipStoreTarget(State, Target, &Value);
                free(Target);
                free(Right);
                free(Left);

                break;
            }

            /* DefToBuffer := ToBufferOp Operand Target */
            case 0x96: {
                AcpiValue *Operand = AcpipExecuteTermArg(State);
                if (!Operand) {
                    return NULL;
                }

                AcpipTarget *Target = AcpipExecuteTarget(State);
                if (!Target) {
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_BUFFER;

                /* Strings mostly pass through, except for 0-length, they just become 0-sized
                   buffers (instead of length==1). */
                if (Operand->Type == ACPI_STRING) {
                    size_t StringSize = strlen(Operand->String);
                    if (StringSize) {
                        Value.Buffer.Size = StringSize + 1;
                        Value.Buffer.Data = (uint8_t *)strdup(Operand->String);
                        if (!Value.Buffer.Data) {
                            free(Target);
                            free(Operand);
                            return NULL;
                        }
                    }
                }

                /* Buffers just pass straight through (no copy is done if its 0-sized). */
                if (Operand->Type == ACPI_BUFFER && Operand->Buffer.Size) {
                    Value.Buffer.Size = Operand->Buffer.Size;
                    Value.Buffer.Data = malloc(Operand->Buffer.Size);

                    if (!Value.Buffer.Data) {
                        free(Target);
                        free(Operand);
                        return NULL;
                    }

                    memcpy(Value.Buffer.Data, Operand->Buffer.Data, Operand->Buffer.Size);
                }

                /* At last, integers get their underlying in-memory representation copied as an
                   8-byte buffer. */
                if (Operand->Type == ACPI_INTEGER) {
                    Value.Buffer.Size = 16;
                    Value.Buffer.Data = malloc(16);

                    if (!Value.Buffer.Data) {
                        free(Target);
                        free(Operand);
                        return NULL;
                    }

                    *((uint64_t *)Value.Buffer.Data) = Operand->Integer;
                }

                AcpipStoreTarget(State, Target, &Value);
                free(Operand);
                free(Target);

                break;
            }

            /* DefToHexString := ToHexStringOp Operand Target */
            case 0x98: {
                AcpiValue *Operand = AcpipExecuteTermArg(State);
                if (!Operand) {
                    return NULL;
                } else if (
                    Operand->Type != ACPI_INTEGER && Operand->Type != ACPI_STRING &&
                    Operand->Type != ACPI_BUFFER) {
                    free(Operand);
                    return NULL;
                }

                AcpipTarget *Target = AcpipExecuteTarget(State);
                if (!Target) {
                    return NULL;
                }

                AcpiValue Value;
                Value.Type = ACPI_STRING;

                /* Strings pass through even if they aren't really hex strings. */
                if (Operand->Type == ACPI_STRING) {
                    Value.String = strdup(Operand->String);
                    if (!Value.String) {
                        free(Target);
                        free(Operand);
                        return NULL;
                    }
                }

                /* Integers get converted using snprintf (just to make sure we're not overflowing
                   the buffer). */
                if (Operand->Type == ACPI_INTEGER) {
                    Value.String = malloc(17);
                    if (!Value.String) {
                        free(Target);
                        free(Operand);
                        return NULL;
                    }

                    if (snprintf(Value.String, 17, "%016llX", Operand->Integer) != 16) {
                        free(Value.String);
                        free(Target);
                        free(Operand);
                        return NULL;
                    }
                }

                /* At last/remaining, we have buffers; They become a list of comma separated
                   2-digit hex strings.  */
                if (Operand->Type == ACPI_BUFFER) {
                    Value.String = malloc(Operand->Buffer.Size * 5);
                    if (!Value.String) {
                        free(Target);
                        free(Operand);
                        return NULL;
                    }

                    for (uint64_t i = 0; i < Operand->Buffer.Size; i++) {
                        int PrintfSize = 5;
                        const char *PrintfFormat = "0x%02hhX";

                        if (i < Operand->Buffer.Size - 1) {
                            PrintfSize = 6;
                            PrintfFormat = "0x%02hhX,";
                        }

                        if (snprintf(
                                Value.String + i * 5,
                                PrintfSize,
                                PrintfFormat,
                                Operand->Buffer.Data[i]) != PrintfSize - 1) {
                            free(Value.String);
                            free(Target);
                            free(Operand);
                            return NULL;
                        }
                    }
                }

                AcpipStoreTarget(State, Target, &Value);
                free(Operand);
                free(Target);

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
