/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AcpiValue *AcpipExecuteTermArg(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return NULL;
    }

    AcpiValue *Value = calloc(1, sizeof(AcpiValue));
    if (!Value) {
        return NULL;
    }

    uint32_t Start = State->RemainingLength;
    switch (Opcode) {
        /* ZeroOp */
        case 0x00: {
            Value->Type = ACPI_VALUE_INTEGER;
            Value->Integer = 0;
            break;
        }

        /* OneOp */
        case 0x01: {
            Value->Type = ACPI_VALUE_INTEGER;
            Value->Integer = 1;
            break;
        }

        /* ByteConst := BytePrefix ByteData */
        case 0x0A: {
            Value->Type = ACPI_VALUE_INTEGER;

            if (!AcpipReadByte(State, (uint8_t *)&Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* WordConst := WordPrefix WordData */
        case 0x0B: {
            Value->Type = ACPI_VALUE_INTEGER;

            if (!AcpipReadWord(State, (uint16_t *)&Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* DWordConst := DWordPrefix DWordData */
        case 0x0C: {
            Value->Type = ACPI_VALUE_INTEGER;

            if (!AcpipReadDWord(State, (uint32_t *)&Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* String := StringPrefix AsciiCharList NullChar */
        case 0x0D: {
            Value->Type = ACPI_VALUE_STRING;

            size_t StringSize = 0;
            while (StringSize < State->RemainingLength && State->Code[StringSize]) {
                StringSize++;
            }

            if (StringSize > State->RemainingLength) {
                free(Value);
                return NULL;
            }

            Value->String = malloc(StringSize);
            if (!Value->String) {
                free(Value);
                return NULL;
            }

            memcpy(Value->String, State->Code, StringSize);
            State->Code += StringSize + 1;
            State->RemainingLength -= StringSize + 1;

            break;
        }

        /* QWordConst := QWordPrefix QWordData */
        case 0x0E: {
            Value->Type = ACPI_VALUE_INTEGER;

            if (!AcpipReadQWord(State, &Value->Integer)) {
                free(Value);
                return NULL;
            }

            break;
        }

        /* DefBuffer := BufferOp PkgLength BufferSize ByteList */
        case 0x11: {
            Value->Type = ACPI_VALUE_BUFFER;

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
            } else if (BufferSize->Type != ACPI_VALUE_INTEGER) {
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

            uint32_t LengthSoFar = Start - State->RemainingLength;
            if (LengthSoFar > PkgLength || PkgLength - LengthSoFar > State->RemainingLength) {
                free(Value->Buffer.Data);
                free(Value);
                return NULL;
            }

            for (uint32_t i = 0; i < PkgLength - LengthSoFar; i++) {
                Value->Buffer.Data[i] = *(State->Code++);
                State->RemainingLength--;
            }

            break;
        }

        /* OnesOp */
        case 0xFF: {
            Value->Type = ACPI_VALUE_INTEGER;
            Value->Integer = UINT64_MAX;
            break;
        }

        default:
            printf(
                "unimplemented termarg opcode: %#hx; %d bytes left to parse out of %d.\n",
                Opcode,
                State->RemainingLength,
                State->Length);
            return NULL;
    }

    return Value;
}
