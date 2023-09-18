/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* TODO: Doing it this way doesn't quite look right. */

AcpipTarget *AcpipExecuteSuperName(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return NULL;
    }

    AcpipTarget *Target = malloc(sizeof(AcpipTarget));
    if (!Target) {
        return NULL;
    }

    switch (Opcode) {
        /* LocalObj (Local0-6) */
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
            Target->Type = ACPI_TARGET_LOCAL;
            Target->LocalIndex = Opcode - 0x60;
            break;

        default: {
            printf(
                "unimplemented supername opcode: %#hhx; %d bytes left to parse out of %d.\n",
                Opcode,
                State->Scope->RemainingLength,
                State->Scope->Length);
            while (1)
                ;
        }
    }

    return Target;
}

AcpipTarget *AcpipExecuteTarget(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return NULL;
    }

    AcpipTarget *Target = malloc(sizeof(AcpipTarget));
    if (!Target) {
        return NULL;
    }

    switch (Opcode) {
        /* Assume writes to constant objects are to be ignore. */
        case 0x00:
        case 0x01:
        case 0x0F:
            Target->Type = ACPI_TARGET_NONE;
            break;

        /* LocalObj (Local0-6) */
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x63:
        case 0x64:
        case 0x65:
        case 0x66:
            Target->Type = ACPI_TARGET_LOCAL;
            Target->LocalIndex = Opcode - 0x60;
            break;

        default: {
            printf(
                "unimplemented target opcode: %#hhx; %d bytes left to parse out of %d.\n",
                Opcode,
                State->Scope->RemainingLength,
                State->Scope->Length);
            while (1)
                ;
        }
    }

    return Target;
}

int AcpipReadTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value) {
    AcpiValue *Source;

    switch (Target->Type) {
        case ACPI_TARGET_LOCAL:
            Source = &State->Locals[Target->LocalIndex];
            break;
        default:
            return 0;
    }

    return AcpiCopyValue(Source, Value);
}

void AcpipStoreTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value) {
    switch (Target->Type) {
        case ACPI_TARGET_LOCAL:
            memcpy(&State->Locals[Target->LocalIndex], Value, sizeof(AcpiValue));
            break;
    }
}
