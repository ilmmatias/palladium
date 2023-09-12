/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void FreeElements(AcpiValue *Value, int Size) {
    for (uint8_t i = 0; i < Size; i++) {
        if (Value->Package.Data[i].Type) {
            free(Value->Package.Data[i].Value);
        } else {
            free(Value->Package.Data[i].String);
        }
    }

    free(Value->Package.Data);
    free(Value);
}

AcpiValue *AcpipExecuteTermArg(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return NULL;
    }

    AcpiValue *Value = calloc(1, sizeof(AcpiValue));
    if (!Value) {
        return NULL;
    }

    uint32_t Start = State->Scope->RemainingLength;
    switch (Opcode) {
        /* ZeroOp */
        case 0x00: {
            Value->Type = ACPI_INTEGER;
            Value->Integer = 0;
            break;
        }

        /* OneOp */
        case 0x01: {
            Value->Type = ACPI_INTEGER;
            Value->Integer = 1;
            break;
        }

        /* ByteConst := BytePrefix ByteData */
        case 0x0A: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadByte(State, (uint8_t *)&Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* WordConst := WordPrefix WordData */
        case 0x0B: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadWord(State, (uint16_t *)&Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* DWordConst := DWordPrefix DWordData */
        case 0x0C: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadDWord(State, (uint32_t *)&Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* String := StringPrefix AsciiCharList NullChar */
        case 0x0D: {
            Value->Type = ACPI_STRING;

            size_t StringSize = 0;
            while (StringSize < State->Scope->RemainingLength && State->Scope->Code[StringSize]) {
                StringSize++;
            }

            if (StringSize > State->Scope->RemainingLength) {
                free(Value);
                return NULL;
            }

            Value->String = malloc(StringSize);
            if (!Value->String) {
                free(Value);
                return NULL;
            }

            memcpy(Value->String, State->Scope->Code, StringSize);
            State->Scope->Code += StringSize + 1;
            State->Scope->RemainingLength -= StringSize + 1;

            break;
        }

        /* QWordConst := QWordPrefix QWordData */
        case 0x0E: {
            Value->Type = ACPI_INTEGER;

            if (!AcpipReadQWord(State, &Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* DefBuffer := BufferOp PkgLength BufferSize ByteList */
        case 0x11: {
            Value->Type = ACPI_BUFFER;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength)) {
                free(Value);
                return NULL;
            }

            /* BufferSize should always be coerced into an integer; If it is anything else, we
               have invalid AML code. */
            AcpiValue *BufferSize = AcpipExecuteTermArg(State);
            if (!BufferSize) {
                free(Value);
                return NULL;
            } else if (BufferSize->Type != ACPI_INTEGER) {
                free(BufferSize);
                free(Value);
                return NULL;
            }

            Value->Buffer.Size = BufferSize->Integer;
            free(BufferSize);

            Value->Buffer.Data = calloc(1, Value->Buffer.Size);
            if (!Value->Buffer.Data) {
                free(Value);
                return NULL;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength ||
                PkgLength - LengthSoFar > Value->Buffer.Size) {
                free(Value->Buffer.Data);
                free(Value);
                return NULL;
            }

            for (uint32_t i = 0; i < PkgLength - LengthSoFar; i++) {
                Value->Buffer.Data[i] = *(State->Scope->Code++);
                State->Scope->RemainingLength--;
            }

            break;
        }

        /* DefPackage := PackageOp PkgLength NumElements PackageElementList */
        case 0x12: {
            Value->Type = ACPI_PACKAGE;

            uint32_t PkgLength;
            if (!AcpipReadPkgLength(State, &PkgLength) ||
                !AcpipReadByte(State, &Value->Package.Size)) {
                free(Value);
                return NULL;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar >= PkgLength ||
                PkgLength - LengthSoFar > State->Scope->RemainingLength) {
                free(Value);
                return NULL;
            }

            PkgLength -= LengthSoFar;
            Value->Package.Data = calloc(Value->Package.Size, sizeof(AcpiPackageElement));
            if (!Value->Package.Data) {
                free(Value);
                return NULL;
            }

            uint8_t i = 0;
            while (PkgLength) {
                if (i >= Value->Package.Size) {
                    FreeElements(Value, i);
                    return NULL;
                }

                uint32_t Start = State->Scope->RemainingLength;
                uint8_t Opcode = *State->Scope->Code;
                uint8_t ExtOpcode = 0;

                if (Opcode == 0x5B) {
                    if (State->Scope->RemainingLength < 2) {
                        FreeElements(Value, i);
                        return NULL;
                    }

                    ExtOpcode = *(State->Scope->Code + 1);
                }

                /* Each PackageElement is either a DataRefObject (which we just call ExecuteTermArg
                   to handle), or a NameString; We just determine if we're a DataRefObject, and if
                   not, call ReadName. */
                if (Opcode <= 0x01 || Opcode == 0x0A || Opcode == 0x0B || Opcode == 0x0C ||
                    Opcode == 0x0D || Opcode == 0x0E || Opcode == 0x11 || Opcode == 0x12 ||
                    Opcode == 0x13 || Opcode == 0xFF || (Opcode == 0x5B && ExtOpcode == 0x30)) {
                    Value->Package.Data[i].Type = 1;
                    Value->Package.Data[i].Value = AcpipExecuteTermArg(State);
                    if (!Value->Package.Data[i].Value) {
                        FreeElements(Value, i);
                        return NULL;
                    }
                } else if (!AcpipReadName(State)) {
                    FreeElements(Value, i);
                    return NULL;
                }

                i++;
                if (Start - State->Scope->RemainingLength > PkgLength) {
                    FreeElements(Value, i);
                    return NULL;
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
            memcpy(Value, &State->Locals[Opcode - 0x60], sizeof(AcpiValue));
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
            memcpy(Value, &State->Arguments[Opcode - 0x68], sizeof(AcpiValue));
            break;
        }

        /* DefSizeOf := SizeOfOp SuperName */
        case 0x87: {
            AcpipTarget *SuperName = AcpipExecuteSuperName(State);
            if (!SuperName) {
                free(Value);
                return NULL;
            }

            AcpiValue *Target = AcpipReadTarget(State, SuperName);
            free(SuperName);
            if (!Target) {
                free(Value);
                return NULL;
            }

            Value->Type = ACPI_INTEGER;
            if (Target->Type == ACPI_STRING) {
                Value->Integer = strlen(Target->String);
            } else if (Target->Type == ACPI_BUFFER) {
                Value->Integer = Target->Buffer.Size;
            } else if (Target->Type == ACPI_PACKAGE) {
                Value->Integer = Target->Package.Size;
            } else {
                free(Value);
                return NULL;
            }

            break;
        }

        /* OnesOp */
        case 0xFF: {
            Value->Type = ACPI_INTEGER;
            Value->Integer = UINT64_MAX;
            break;
        }

        default: {
            printf(
                "unimplemented termarg opcode: %#hhx; %d bytes left to parse out of %d.\n",
                Opcode,
                State->Scope->RemainingLength,
                State->Scope->Length);
            while (1)
                ;
        }
    }

    return Value;
}
