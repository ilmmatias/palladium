/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

AcpiValue *AcpipExecuteTermArg(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return NULL;
    }

    AcpiValue *Value = calloc(1, sizeof(AcpiValue));
    if (!Value) {
        return NULL;
    }

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

        /* QWordConst := QWordPrefix QWordData */
        case 0x0E: {
            Value->Type = ACPI_VALUE_INTEGER;

            if (!AcpipReadQWord(State, &Value->Integer)) {
                free(Value);
                return NULL;
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
            printf("unimplemented opcode: %#hx\n", Opcode);
            return NULL;
    }

    return Value;
}
