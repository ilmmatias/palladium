/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdlib.h>
#include <string.h>

int AcpipReadTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value) {
    AcpiValue *Source;

    switch (Target->Type) {
        case ACPI_LOCAL:
            Source = &State->Locals[Target->Integer];
            break;
        case ACPI_ARG:
            Source = &State->Arguments[Target->Integer];
            break;
        case ACPI_REFERENCE:
            AcpiCreateReference(&Target->Reference->Value, Value);
            return 1;
        default:
            return 0;
    }

    return AcpiCopyValue(Source, Value);
}

int AcpipStoreTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value) {
    switch (Target->Type) {
        case ACPI_LOCAL:
            AcpiRemoveReference(&State->Locals[Target->Integer], 0);
            memcpy(&State->Locals[Target->Integer], Value, sizeof(AcpiValue));
            break;
        case ACPI_ARG:
            AcpiRemoveReference(&State->Arguments[Target->Integer], 0);
            memcpy(&State->Arguments[Target->Integer], Value, sizeof(AcpiValue));
            break;
        case ACPI_DEBUG:
            if (AcpipCastToString(Value, 1, 0)) {
                AcpipShowDebugMessage("AML message: %s\n", Value->String->Data);
                AcpiRemoveReference(Value, 0);
            }

            break;
        case ACPI_REFERENCE:
            switch (Target->Reference->Value.Type) {
                /* Integers, strings, and buffers are allowed to be implicitly cast into. */
                case ACPI_INTEGER: {
                    uint64_t IntegerValue;
                    if (!AcpipCastToInteger(Value, &IntegerValue, 1)) {
                        return 0;
                    }

                    AcpiRemoveReference(&Target->Reference->Value, 0);
                    Target->Reference->Value.Type = ACPI_INTEGER;
                    Target->Reference->Value.Integer = IntegerValue;

                    break;
                }

                case ACPI_STRING: {
                    AcpiRemoveReference(&Target->Reference->Value, 0);
                    memcpy(&Target->Reference->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToString(&Target->Reference->Value, 1, 0)) {
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    AcpiRemoveReference(&Target->Reference->Value, 0);
                    memcpy(&Target->Reference->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToBuffer(&Target->Reference->Value)) {
                        return 0;
                    }

                    break;
                }

                /* Store to packages is only allowed if the source too is a package. */
                case ACPI_PACKAGE: {
                    if (Value->Type != ACPI_PACKAGE) {
                        return 0;
                    }

                    AcpiRemoveReference(&Target->Reference->Value, 0);
                    memcpy(&Target->Reference->Value, Value, sizeof(AcpiValue));

                    break;
                }

                /* Store to fields is only allowed for buffer-like sources (integers, strings, and
                   buffers). */
                case ACPI_FIELD_UNIT: {
                    if (Value->Type != ACPI_INTEGER && Value->Type != ACPI_STRING &&
                        Value->Type != ACPI_BUFFER) {
                        return 0;
                    }

                    int Status = AcpipWriteField(&Target->Reference->Value, Value);
                    AcpiRemoveReference(Value, 0);

                    if (!Status) {
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER_FIELD: {
                    uint64_t Index = Target->Reference->Value.BufferField.Index;
                    AcpiValue *Source = Target->Reference->Value.BufferField.Source;

                    uint64_t Slot;
                    if (!AcpipCastToInteger(Value, &Slot, 1)) {
                        return 0;
                    }

                    switch (Target->Reference->Value.BufferField.Size) {
                        case 2:
                            *((uint16_t *)(Source->Buffer->Data + Index)) = Slot;
                            break;
                        case 4:
                            *((uint32_t *)(Source->Buffer->Data + Index)) = Slot;
                            break;
                        case 8:
                            *((uint64_t *)(Source->Buffer->Data + Index)) = Slot;
                            break;
                        default:
                            *(Source->Buffer->Data + Index) = Slot;
                            break;
                    }

                    break;
                }

                default:
                    AcpipShowErrorMessage(
                        ACPI_REASON_CORRUPTED_TABLES,
                        "writing to a named field of type %d\n",
                        Target->Reference->Value.Type);
            }
    }

    return 1;
}
