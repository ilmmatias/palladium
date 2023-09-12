/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cleans up the elements of a DefPackage after a failure.
 *
 * PARAMETERS:
 *     Value - Value pointer containing the package.
 *     Size - How many items were already wrote.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void FreeElements(AcpiValue *Value, int Size) {
    for (uint8_t i = 0; i < Size; i++) {
        if (Value->Package.Data[i].Type) {
            AcpiFreeValueData(&Value->Package.Data[i].Value);
        }
    }

    free(Value->Package.Data);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes the whichever AML opcode the current scope points to, updating the
 *     internal state accordingly.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Result - Output; Where to store the resulting value; Only used for expression calls.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteOpcode(AcpipState *State, AcpiValue *Result) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return 0;
    }

    uint8_t ExtOpcode = 0;
    if (Opcode == 0x5B && !AcpipReadByte(State, &ExtOpcode)) {
        return 0;
    }

    uint32_t Start = State->Scope->RemainingLength;
    const uint8_t *StartCode = State->Scope->Code;

    AcpiValue Value;
    memset(&Value, 0, sizeof(AcpiValue));

    switch (Opcode | ((uint16_t)ExtOpcode << 8)) {
        /* ZeroOp */
        case 0x00: {
            Value.Type = ACPI_INTEGER;
            Value.Integer = 0;
            break;
        }

        /* OneOp */
        case 0x01: {
            Value.Type = ACPI_INTEGER;
            Value.Integer = 1;
            break;
        }

        /* DefAlias := AliasOp NameString NameString */
        case 0x06: {
            AcpipName *SourceName = AcpipReadName(State);
            if (!SourceName) {
                return 0;
            }

            AcpipName *AliasName = AcpipReadName(State);
            if (!AliasName) {
                free(SourceName);
                return 0;
            }

            AcpiObject *SourceObject = AcpipResolveObject(SourceName);
            if (!SourceObject) {
                free(AliasName);
                free(SourceName);
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_ALIAS;
            Value.Alias = SourceObject;

            AcpiObject *Object = AcpipCreateObject(AliasName, &Value);
            if (!Object) {
                free(AliasName);
                return 0;
            }

            break;
        }

        /* DefName := NameOp NameString DataRefObject */
        case 0x08: {
            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            AcpiValue DataRefObject;
            if (!AcpipExecuteOpcode(State, &DataRefObject)) {
                free(Name);
                return 0;
            }

            AcpiObject *Object = AcpipCreateObject(Name, &DataRefObject);
            if (!Object) {
                AcpiFreeValueData(&DataRefObject);
                free(Name);
                return 0;
            }

            break;
        }

        /* ByteConst := BytePrefix ByteData */
        case 0x0A: {
            Value.Type = ACPI_INTEGER;

            if (!AcpipReadByte(State, (uint8_t *)&Value.Integer)) {
                return 0;
            }

            break;
        }

        /* WordConst := WordPrefix WordData */
        case 0x0B: {
            Value.Type = ACPI_INTEGER;

            if (!AcpipReadWord(State, (uint16_t *)&Value.Integer)) {
                return 0;
            }

            break;
        }

        /* DWordConst := DWordPrefix DWordData */
        case 0x0C: {
            Value.Type = ACPI_INTEGER;

            if (!AcpipReadDWord(State, (uint32_t *)&Value.Integer)) {
                return 0;
            }

            break;
        }

        /* String := StringPrefix AsciiCharList NullChar */
        case 0x0D: {
            Value.Type = ACPI_STRING;

            size_t StringSize = 0;
            while (StringSize < State->Scope->RemainingLength && State->Scope->Code[StringSize]) {
                StringSize++;
            }

            if (StringSize > State->Scope->RemainingLength) {
                return 0;
            }

            Value.String = malloc(StringSize);
            if (!Value.String) {
                return 0;
            }

            memcpy(Value.String, State->Scope->Code, StringSize);
            State->Scope->Code += StringSize + 1;
            State->Scope->RemainingLength -= StringSize + 1;

            break;
        }

        /* QWordConst := QWordPrefix QWordData */
        case 0x0E: {
            Value.Type = ACPI_INTEGER;

            if (!AcpipReadQWord(State, &Value.Integer)) {
                return 0;
            }

            break;
        }

        /* DefScope := ScopeOp PkgLength NameString TermList
         * DefDevice := DeviceOp PkgLength NameString TermList */
        case 0x10:
        case 0x825B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                free(Name);
                return 0;
            }

            AcpiValue Value;
            Value.Type = Opcode == 0x10 ? ACPI_SCOPE : ACPI_DEVICE;
            Value.Objects = NULL;

            AcpiObject *Object = AcpipCreateObject(Name, &Value);
            if (!Object) {
                free(Name);
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefBuffer := BufferOp PkgLength BufferSize ByteList */
        case 0x11: {
            Value.Type = ACPI_BUFFER;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength)) {
                return 0;
            } else if (!AcpipExecuteInteger(State, &Value.Buffer.Size)) {
                return 0;
            }

            Value.Buffer.Data = calloc(1, Value.Buffer.Size);
            if (!Value.Buffer.Data) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength ||
                PkgLength - LengthSoFar > Value.Buffer.Size) {
                free(Value.Buffer.Data);
                return 0;
            }

            for (uint32_t i = 0; i < PkgLength - LengthSoFar; i++) {
                Value.Buffer.Data[i] = *(State->Scope->Code++);
                State->Scope->RemainingLength--;
            }

            break;
        }

        /* DefPackage := PackageOp PkgLength NumElements PackageElementList */
        case 0x12: {
            Value.Type = ACPI_PACKAGE;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength) ||
                !AcpipReadByte(State, &Value.Package.Size)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar >= PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            PkgLength -= LengthSoFar;
            Value.Package.Data = calloc(Value.Package.Size, sizeof(AcpiPackageElement));
            if (!Value.Package.Data) {
                return 0;
            }

            uint8_t i = 0;
            while (PkgLength) {
                if (i >= Value.Package.Size) {
                    FreeElements(&Value, i);
                    return 0;
                }

                uint32_t Start = State->Scope->RemainingLength;
                uint8_t Opcode = *State->Scope->Code;

                /* Each PackageElement should always be either a DataRefObject (which we just call
                   ExecuteOpcode to handle), or a NameString. */
                if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                    !isupper(Opcode) && Opcode != '_') {
                    Value.Package.Data[i].Type = 1;
                    if (!AcpipExecuteOpcode(State, &Value.Package.Data[i].Value)) {
                        FreeElements(&Value, i);
                        return 0;
                    }
                } else {
                    if (!AcpipReadName(State)) {
                        FreeElements(&Value, i);
                        return 0;
                    }
                }

                i++;
                if (Start - State->Scope->RemainingLength > PkgLength) {
                    FreeElements(&Value, i);
                    return 0;
                }

                PkgLength -= Start - State->Scope->RemainingLength;
            }

            break;
        }

        /* DefMethod := MethodOp PkgLength NameString MethodFlags TermList */
        case 0x14: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar >= Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                free(Name);
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_METHOD;
            Value.Method.Start = State->Scope->Code + 1;
            Value.Method.Size = Length - LengthSoFar;
            Value.Method.Flags = *State->Scope->Code;

            if (!AcpipCreateObject(Name, &Value)) {
                free(Name);
                return 0;
            }

            State->Scope->Code += Length - LengthSoFar;
            State->Scope->RemainingLength -= Length - LengthSoFar;
            break;
        }

        /* LocalObj (Local0-6) */
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66: {
            memcpy(&Value, &State->Locals[Opcode - 0x60], sizeof(AcpiValue));
            break;
        }

        /* ArgObj (Arg0-6) */
        case 0x68:
        case 0x69:
        case 0x6A:
        case 0x6B:
        case 0x6C:
        case 0x6D:
        case 0x6E: {
            memcpy(&Value, &State->Arguments[Opcode - 0x68], sizeof(AcpiValue));
            break;
        }

        /* DefStore := StoreOp TermArg SuperName */
        case 0x70: {
            if (!AcpipExecuteOpcode(State, &Value)) {
                return 0;
            }

            AcpipTarget *Target = AcpipExecuteSuperName(State);
            if (!Target) {
                AcpiFreeValueData(&Value);
                return 0;
            }

            AcpipStoreTarget(State, Target, &Value);
            free(Target);

            break;
        }

        /* Binary operations with target (all follow the format: Op Operand Operand Target). */
        case 0x72:
        case 0x74:
        case 0x77:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        case 0x85: {
            uint64_t Left;
            if (!AcpipExecuteInteger(State, &Left)) {
                return 0;
            }

            uint64_t Right;
            if (!AcpipExecuteInteger(State, &Right)) {
                return 0;
            }

            AcpipTarget *Target = AcpipExecuteTarget(State);
            if (!Target) {
                return 0;
            }

            Value.Type = ACPI_INTEGER;
            switch (Opcode) {
                case 0x72:
                    Value.Integer = Left + Right;
                    break;
                case 0x74:
                    Value.Integer = Left - Right;
                    break;
                case 0x77:
                    Value.Integer = Left * Right;
                    break;
                case 0x79:
                    Value.Integer = Left << Right;
                    break;
                case 0x7A:
                    Value.Integer = Left >> Right;
                    break;
                case 0x7B:
                    Value.Integer = Left & Right;
                    break;
                case 0x7C:
                    Value.Integer = Left & ~Right;
                    break;
                case 0x7D:
                    Value.Integer = Left | Right;
                    break;
                case 0x7E:
                    Value.Integer = Left | ~Right;
                    break;
                case 0x7F:
                    Value.Integer = Left ^ Right;
                    break;
                case 0x85:
                    Value.Integer = Left % Right;
                    break;
            }

            AcpipStoreTarget(State, Target, &Value);
            free(Target);

            break;
        }

        /* DefConcat := ConcatOp Data Data Target */
        case 0x73: {
            AcpiValue Left;
            if (!AcpipExecuteOpcode(State, &Left)) {
                return 0;
            }

            AcpiValue Right;
            if (!AcpipExecuteOpcode(State, &Right)) {
                AcpiFreeValueData(&Left);
                return 0;
            }

            AcpipTarget *Target = AcpipExecuteTarget(State);
            if (!Target) {
                AcpiFreeValueData(&Left);
                AcpiFreeValueData(&Right);
                return 0;
            }

            switch (Left.Type) {
                /* Read it as two integers, and append them into a buffer. */
                case ACPI_INTEGER: {
                    uint64_t LeftValue = Left.Integer;
                    uint64_t RightValue;
                    int Result = AcpipCastToInteger(&Right, &RightValue);

                    AcpiFreeValueData(&Left);
                    AcpiFreeValueData(&Right);
                    if (!Result) {
                        return 0;
                    }

                    Value.Type = ACPI_BUFFER;
                    Value.Buffer.Size = 16;
                    Value.Buffer.Data = malloc(16);
                    if (!Value.Buffer.Data) {
                        return 0;
                    }

                    *((uint64_t *)Value.Buffer.Data) = LeftValue;
                    *((uint64_t *)(Value.Buffer.Data + 8)) = RightValue;

                    break;
                }

                /* Read it as two buffers, and append them into another buffer. */
                case ACPI_BUFFER: {
                    if (!AcpipCastToBuffer(&Right)) {
                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);
                        return 0;
                    }

                    Value.Type = ACPI_BUFFER;
                    Value.Buffer.Size = Left.Buffer.Size + Right.Buffer.Size;
                    Value.Buffer.Data = malloc(Value.Buffer.Size);
                    if (!Value.Buffer.Data) {
                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);
                        return 0;
                    }

                    memcpy(Value.Buffer.Data, Left.Buffer.Data, Left.Buffer.Size);
                    memcpy(
                        Value.Buffer.Data + Left.Buffer.Size, Right.Buffer.Data, Right.Buffer.Size);
                    AcpiFreeValueData(&Left);
                    AcpiFreeValueData(&Right);

                    break;
                }

                /* Convert both sides into strings, and append them into a single string. */
                default: {
                    if (!AcpipCastToString(&Left, 1) || !AcpipCastToString(&Right, 1)) {
                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);
                        return 0;
                    }

                    Value.Type = ACPI_STRING;
                    Value.String = malloc(strlen(Left.String) + strlen(Right.String) + 1);
                    if (!Value.String) {
                        AcpiFreeValueData(&Left);
                        AcpiFreeValueData(&Right);
                        return 0;
                    }

                    strcpy(Value.String, Left.String);
                    strcat(Value.String, Right.String);
                    AcpiFreeValueData(&Left);
                    AcpiFreeValueData(&Right);

                    break;
                }
            }

            AcpipStoreTarget(State, Target, &Value);
            free(Target);

            break;
        }

        /* DefToBuffer := ToBufferOp Operand Target */
        case 0x96: {
            if (!AcpipExecuteOpcode(State, &Value)) {
                return 0;
            }

            AcpipTarget *Target = AcpipExecuteTarget(State);
            if (!Target) {
                AcpiFreeValueData(&Value);
                return 0;
            }

            if (!AcpipCastToBuffer(&Value)) {
                free(Target);
                AcpiFreeValueData(&Value);
                return 0;
            }

            AcpipStoreTarget(State, Target, &Value);
            free(Target);

            break;
        }

        /* DefToHexString := ToHexStringOp Operand Target */
        case 0x98: {
            if (!AcpipExecuteOpcode(State, &Value)) {
                return 0;
            }

            AcpipTarget *Target = AcpipExecuteTarget(State);
            if (!Target) {
                AcpiFreeValueData(&Value);
                return 0;
            }

            if (!AcpipCastToString(&Value, 0)) {
                free(Target);
                AcpiFreeValueData(&Value);
                return 0;
            }

            AcpipStoreTarget(State, Target, &Value);
            free(Target);

            break;
        }

        /* DefIfElse := IfOp PkgLength Predicate TermList DefElse */
        case 0xA0: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            uint64_t Predicate;
            if (!AcpipExecuteInteger(State, &Predicate)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            if (Predicate) {
                AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
                if (!Scope) {
                    return 0;
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
                return 0;
            }

            LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefElse := ElseOp PkgLength TermList */
        case 0xA1: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length) || Length > Start) {
                return 0;
            }

            State->Scope->Code = StartCode + Length;
            State->Scope->RemainingLength = Start - Length;
            break;
        }

        /* DefWhile := WhileOp PkgLength Predicate TermList */
        case 0xA2: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            const uint8_t *PredicateStart = State->Scope->Code;
            uint32_t PredicateBacktrack = State->Scope->RemainingLength;

            uint64_t Predicate;
            if (!AcpipExecuteInteger(State, &Predicate)) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            if (!Predicate) {
                State->Scope->Code += Length - LengthSoFar;
                State->Scope->RemainingLength -= Length - LengthSoFar;
                break;
            }

            AcpipScope *Scope =
                AcpipEnterWhile(State, PredicateStart, PredicateBacktrack, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            break;
        }

        /* DefMutex := MutexOp NameString SyncFlags */
        case 0x015B: {
            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint8_t SyncFlags;
            if (!AcpipReadByte(State, &SyncFlags)) {
                free(Name);
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_MUTEX;
            Value.Mutex.Flags = SyncFlags;

            if (!AcpipCreateObject(Name, &Value)) {
                free(Name);
                return 0;
            }

            break;
        }

        /* DefEvent := EventOp NameString */
        case 0x025B: {
            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_EVENT;

            if (!AcpipCreateObject(Name, &Value)) {
                free(Name);
                return 0;
            }

            break;
        }

        /* RevisionOp := ExtOpPrefix 0x30 */
        case 0x305B: {
            Value.Type = ACPI_INTEGER;
            Value.Integer = ACPI_REVISION;
            break;
        }

        /* DefOpRegion := OpRegionOp NameString RegionSpace RegionOffset RegionLen */
        case 0x805B: {
            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint8_t RegionSpace;
            if (!AcpipReadByte(State, &RegionSpace)) {
                free(Name);
                return 0;
            }

            uint64_t RegionOffset;
            if (!AcpipExecuteInteger(State, &RegionOffset)) {
                free(Name);
                return 0;
            }

            uint64_t RegionLen;
            if (!AcpipExecuteInteger(State, &RegionLen)) {
                free(Name);
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_REGION;
            Value.Objects = NULL;
            Value.Region.RegionSpace = RegionSpace;
            Value.Region.RegionLen = RegionLen;
            Value.Region.RegionOffset = RegionOffset;

            if (!AcpipCreateObject(Name, &Value)) {
                free(Name);
                return 0;
            }

            break;
        }

        /* DefField := FieldOp PkgLength NameString FieldFlags FieldList */
        case 0x815B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            AcpiObject *Object = AcpipResolveObject(Name);
            if (!Object) {
                free(Name);
                return 0;
            } else if (Object->Value.Type != ACPI_REGION) {
                return 0;
            }

            AcpiValue Base;
            Base.Type = ACPI_FIELD_UNIT;
            Base.Field.FieldType = ACPI_FIELD;
            Base.Field.Region = Object;
            if (!AcpipReadFieldList(State, &Base, Start, Length)) {
                return 0;
            }

            break;
        }

        /* DefProcessor := ProcessorOp PkgLength NameString ProcID PblkAddr PblkLen TermList */
        case 0x835B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength + 6;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                free(Name);
                return 0;
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
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefPowerRes := PowerResOp PkgLength NameString SystemLevel ResourceOrder TermList */
        case 0x845B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength + 3;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                free(Name);
                return 0;
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
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefIndexField := IndexFieldOp PkgLength NameString NameString FieldFlags FieldList */
        case 0x865B: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName *IndexName = AcpipReadName(State);
            if (!IndexName) {
                return 0;
            }

            AcpiObject *IndexObject = AcpipResolveObject(IndexName);
            if (!IndexObject) {
                free(IndexName);
                return 0;
            }

            AcpipName *DataName = AcpipReadName(State);
            if (!DataName) {
                free(IndexName);
                return 0;
            }

            AcpiObject *DataObject = AcpipResolveObject(DataName);
            if (!DataObject) {
                free(DataName);
                return 0;
            }

            AcpiValue Base;
            Base.Type = ACPI_FIELD_UNIT;
            Base.Field.FieldType = ACPI_INDEX_FIELD;
            Base.Field.Index = IndexObject;
            Base.Field.Data = DataObject;
            if (!AcpipReadFieldList(State, &Base, Start, Length)) {
                return 0;
            }

            break;
        }

        default: {
            State->Scope->Code--;
            State->Scope->RemainingLength++;

            /* MethodInvocation := NameString TermArgList
               NameString should start with either a prefix (\, ^, 0x2E or 0x2F), or a lead name
               char, so we can quickly check if this is it. */
            if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                !isupper(Opcode) && Opcode != '_') {
                printf(
                    "unimplemented opcode: %#hx; %d bytes left to parse out of %d.\n",
                    Opcode | ((uint32_t)ExtOpcode << 8),
                    State->Scope->RemainingLength,
                    State->Scope->Length);
                while (1)
                    ;
            }

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            AcpiObject *Object = AcpipResolveObject(Name);
            if (!Object) {
                free(Name);
                return 0;
            }

            /* MethodInvocation is valid on non-method items, where we just return their value. */
            if (Object->Value.Type != ACPI_METHOD) {
                if (Object->Value.Type <= ACPI_FIELD_UNIT || Object->Value.Type == ACPI_REFERENCE) {
                    memcpy(&Value, &Object->Value, sizeof(AcpiValue));
                } else {
                    Value.Type = ACPI_REFERENCE;
                    Value.Reference = Object;
                }

                break;
            }

            printf("MethodInvocation\n");
            while (1)
                ;
        }
    }

    if (Result) {
        memcpy(Result, &Value, sizeof(AcpiValue));
    }

    return 1;
}
