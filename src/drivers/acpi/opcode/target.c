/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This functions reads the value stored in a target/name.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Target - Name to read from.
 *     Value - Output; Where to store the value read.
 *
 * RETURN VALUE:
 *     1 if the read was successful, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipReadTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value) {
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
            return true;
        default:
            return false;
    }

    return AcpiCopyValue(Source, Value);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This functions stores a value into a target/name.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Target - Name to store into.
 *     Value - Value to store.
 *
 * RETURN VALUE:
 *     1 if the store was successful, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipStoreTarget(AcpipState *State, AcpiValue *Target, AcpiValue *Value) {
    switch (Target->Type) {
        case ACPI_INDEX: {
            AcpiValue *Buffer = Target->BufferField.Source;
            uint64_t Index = Target->BufferField.Index;

            switch (Buffer->Type) {
                case ACPI_PACKAGE: {
                    AcpiRemoveReference(&Buffer->Package->Data[Index], false);
                    AcpiCopyValue(Value, &Buffer->Package->Data[Index]);
                    break;
                }

                default: {
                    AcpipShowDebugMessage(
                        "attempt to store into index with source type %d\n",
                        Target->BufferField.Source->Type);
                    break;
                }
            }

            break;
        }
        case ACPI_LOCAL:
            AcpiRemoveReference(&State->Locals[Target->Integer], false);
            AcpiCopyValue(Value, &State->Locals[Target->Integer]);
            break;
        case ACPI_ARG:
            AcpiRemoveReference(&State->Arguments[Target->Integer], false);
            AcpiCopyValue(Value, &State->Arguments[Target->Integer]);
            break;
        case ACPI_DEBUG:
            if (AcpipCastToString(Value, true, false)) {
                AcpipShowDebugMessage("AML message: %s\n", Value->String->Data);
                AcpiRemoveReference(Value, false);
            }

            break;
        case ACPI_REFERENCE:
            switch (Target->Reference->Value.Type) {
                /* Integers, strings, and buffers are allowed to be implicitly cast into. */
                case ACPI_INTEGER: {
                    uint64_t IntegerValue;
                    if (!AcpipCastToInteger(Value, &IntegerValue, true)) {
                        return false;
                    }

                    AcpiRemoveReference(&Target->Reference->Value, false);
                    Target->Reference->Value.Type = ACPI_INTEGER;
                    Target->Reference->Value.Integer = IntegerValue;

                    break;
                }

                case ACPI_STRING: {
                    AcpiRemoveReference(&Target->Reference->Value, false);
                    memcpy(&Target->Reference->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToString(&Target->Reference->Value, true, false)) {
                        return false;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    AcpiRemoveReference(&Target->Reference->Value, false);
                    memcpy(&Target->Reference->Value, Value, sizeof(AcpiValue));

                    if (!AcpipCastToBuffer(&Target->Reference->Value)) {
                        return false;
                    }

                    break;
                }

                /* Store to packages is only allowed if the source too is a package. */
                case ACPI_PACKAGE: {
                    if (Value->Type != ACPI_PACKAGE) {
                        return false;
                    }

                    AcpiRemoveReference(&Target->Reference->Value, false);
                    memcpy(&Target->Reference->Value, Value, sizeof(AcpiValue));

                    break;
                }

                /* Store to fields is only allowed for buffer-like sources (integers, strings, and
                   buffers). */
                case ACPI_FIELD_UNIT: {
                    if (Value->Type != ACPI_INTEGER && Value->Type != ACPI_STRING &&
                        Value->Type != ACPI_BUFFER) {
                        return false;
                    }

                    bool Status = AcpipWriteField(&Target->Reference->Value, Value);
                    AcpiRemoveReference(Value, false);

                    if (!Status) {
                        return false;
                    }

                    break;
                }

                case ACPI_BUFFER_FIELD: {
                    uint64_t Index = Target->Reference->Value.BufferField.Index;
                    AcpiValue *Source = Target->Reference->Value.BufferField.Source;
                    int Size = Target->Reference->Value.BufferField.Size;

                    uint64_t Slot;
                    if (!AcpipCastToInteger(Value, &Slot, true)) {
                        return false;
                    }

                    if (Size == 0) {
                        /* Size=0 indicates a 1-bit field where Index is a bit offset. */
                        uint64_t ByteOffset = Index / 8;
                        uint8_t BitOffset = Index % 8;

                        if (ByteOffset >= Source->Buffer->Size) {
                            return false;
                        }

                        if (Slot & 1) {
                            Source->Buffer->Data[ByteOffset] |= (1 << BitOffset);
                        } else {
                            Source->Buffer->Data[ByteOffset] &= ~(1 << BitOffset);
                        }
                    } else {
                        /* Otherwise this is a normal byte/word/dword/qword field. */
                        if (Index + Size > Source->Buffer->Size) {
                            return false;
                        }

                        switch (Size) {
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
                    }

                    break;
                }

                default:
                    AcpipShowErrorMessage(
                        ACPI_REASON_CORRUPTED_TABLES,
                        "writing to a named field of type %d\n",
                        Target->Reference->Value.Type);
            }

            break;
        default:
            if (Target->Type != ACPI_INTEGER) {
                AcpipShowDebugMessage("attempt to store into target of type %d\n", Target->Type);
            }

            break;
    }

    return true;
}
