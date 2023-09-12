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
    // const uint8_t *StartCode = State->Scope->Code;

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

            break;
        }

        /* RevisionOp := ExtOpPrefix 0x30 */
        case 0x305B: {
            Value.Type = ACPI_INTEGER;
            Value.Integer = ACPI_REVISION;
            break;
        }

        default:
            printf(
                "unimplemented opcode: %#hx; %d bytes left to parse out of %d.\n",
                Opcode | ((uint32_t)ExtOpcode << 8),
                State->Scope->RemainingLength,
                State->Scope->Length);
            while (1)
                ;
    }

    if (Result) {
        memcpy(Result, &Value, sizeof(AcpiValue));
    }

    return 1;
}
