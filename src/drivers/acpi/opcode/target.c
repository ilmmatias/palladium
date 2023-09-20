/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ExecuteSuperName(AcpipState *State, uint8_t Opcode, AcpipTarget *Target) {
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

        default:
            /* SimpleName is allowed to be a NameString (store to named field/object). */
            if (Opcode != '\\' && Opcode != '^' && Opcode != 0x2E && Opcode != 0x2F &&
                !isupper(Opcode) && Opcode != '_') {
                return -1;
            }

            State->Scope->Code--;
            State->Scope->RemainingLength++;

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            AcpiObject *Object = AcpipResolveObject(&Name);
            if (!Object) {
                return 0;
            } else if (
                Object->Value.Type > ACPI_FIELD_UNIT && Object->Value.Type != ACPI_BUFFER_FIELD) {
                /* Writing to a scope or method (or internal object) is invalid. */
                return 0;
            }

            Target->Type = ACPI_TARGET_NAMED;
            Target->Object = Object;
            break;
    }

    return 1;
}

AcpipTarget *AcpipExecuteSuperName(AcpipState *State) {
    uint8_t Opcode;
    if (!AcpipReadByte(State, &Opcode)) {
        return NULL;
    }

    AcpipTarget *Target = malloc(sizeof(AcpipTarget));
    if (!Target) {
        return NULL;
    }

    int Status = ExecuteSuperName(State, Opcode, Target);
    if (!Status) {
        free(Target);
        return 0;
    } else if (Status > 0) {
        return Target;
    }

    printf(
        "unimplemented supername opcode: %#hhx; %d bytes left to parse out of %d.\n",
        Opcode,
        State->Scope->RemainingLength,
        State->Scope->Length);
    while (1)
        ;
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

    int Status = ExecuteSuperName(State, Opcode, Target);
    if (!Status) {
        free(Target);
        return 0;
    } else if (Status > 0) {
        return Target;
    }

    switch (Opcode) {
        /* Assume writes to Zero are to be ignored. */
        case 0x00:
            Target->Type = ACPI_TARGET_NONE;
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
        case ACPI_TARGET_NAMED:
            printf("Reading from a named field\n");
            while (1)
                ;
        default:
            return 0;
    }

    return AcpiCopyValue(Source, Value);
}

int AcpipStoreTarget(AcpipState *State, AcpipTarget *Target, AcpiValue *Value) {
    switch (Target->Type) {
        case ACPI_TARGET_LOCAL:
            AcpiRemoveReference(&State->Locals[Target->LocalIndex], 0);
            memcpy(&State->Locals[Target->LocalIndex], Value, sizeof(AcpiValue));
            break;
        case ACPI_TARGET_NAMED:
            switch (Target->Object->Value.Type) {
                /* Integers, strings, and buffers are allowed to be implicitly cast into. */
                case ACPI_INTEGER: {
                    uint64_t IntegerValue;
                    if (!AcpipCastToInteger(Value, &IntegerValue, 1)) {
                        return 0;
                    }

                    AcpiRemoveReference(&Target->Object->Value, 0);
                    Target->Object->Value.Type = ACPI_INTEGER;
                    Target->Object->Value.Integer = IntegerValue;

                    break;
                }

                case ACPI_STRING: {
                    AcpiRemoveReference(&Target->Object->Value, 0);
                    memcpy(&Target->Object->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToString(&Target->Object->Value, 1, 0)) {
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    AcpiRemoveReference(&Target->Object->Value, 0);
                    memcpy(&Target->Object->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToBuffer(&Target->Object->Value)) {
                        return 0;
                    }

                    break;
                }

                /* Store to packages is only allowed if the source too is a package. */
                case ACPI_PACKAGE: {
                    if (Value->Type != ACPI_PACKAGE) {
                        return 0;
                    }

                    AcpiRemoveReference(&Target->Object->Value, 0);
                    memcpy(&Target->Object->Value, Value, sizeof(AcpiValue));

                    break;
                }

                /* Store to fields is only allowed for buffer-like sources (integers, strings, and
                   buffers). */
                case ACPI_FIELD_UNIT: {
                    if (Value->Type != ACPI_INTEGER && Value->Type != ACPI_STRING &&
                        Value->Type != ACPI_BUFFER) {
                        return 0;
                    }

                    int Status = AcpipWriteField(&Target->Object->Value, Value);
                    AcpiRemoveReference(Value, 0);

                    if (!Status) {
                        return 0;
                    }

                    break;
                }

                default:
                    printf("Writing to a named field of type %d\n", Target->Object->Value.Type);
                    while (1)
                        ;
            }
    }

    return 1;
}
