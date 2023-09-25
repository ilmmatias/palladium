/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a reference manipulation opcode.
 *     This contains all ops related to creating, dereferencing, and manipulating object
 *     references.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *     Value - Output; Where to store the resulting Value.
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "try another Execute* fn", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteRefOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value) {
    switch (Opcode) {
        /* DerefOf := DerefOfOp ObjReference */
        case 0x83: {
            AcpiValue Reference;
            if (!AcpipExecuteOpcode(State, &Reference, 1)) {
                return 0;
            }

            switch (Reference.Type) {
                case ACPI_INDEX: {
                    AcpiValue *Source = Reference.Index.Source;
                    uint64_t Index = Reference.Index.Index;

                    if (Source->Type == ACPI_PACKAGE) {
                        if (!Source->Package->Data[Index].Type) {
                            AcpipShowErrorMessage(
                                ACPI_REASON_CORRUPTED_TABLES,
                                "ACPI_PACKAGE, Type == 0 (string), TODO!\n");
                        }

                        AcpiCreateReference(&Source->Package->Data[Index].Value, Value);
                    } else {
                        Value->Type = ACPI_INTEGER;
                        Value->References = 1;
                        Value->Integer = Source->Type == ACPI_STRING ? Source->String->Data[Index]
                                                                     : Source->Buffer->Data[Index];
                    }

                    break;
                }

                case ACPI_LOCAL:
                    AcpiCreateReference(&State->Locals[Reference.Integer], Value);
                    break;

                case ACPI_ARG:
                    AcpiCreateReference(&State->Arguments[Reference.Integer], Value);
                    break;

                case ACPI_REFERENCE:
                    AcpiCreateReference(&Reference.Reference->Value, Value);
                    break;

                default:
                    AcpiRemoveReference(&Reference, 0);
                    return 0;
            }

            AcpiRemoveReference(&Reference, 0);
            break;
        }

        /* DefIndex := IndexOp BuffPkgStrObj IndexValue Target */
        case 0x88: {
            /* BufferField takes a reference to an AcpiValue (that needs to live for long
               enough), so allocate some memory for it. */
            AcpiValue *Buffer = malloc(sizeof(AcpiValue));
            if (!Buffer) {
                return 0;
            } else if (!AcpipExecuteOpcode(State, Buffer, 0)) {
                free(Buffer);
                return 0;
            }

            if (Buffer->Type != ACPI_STRING && Buffer->Type != ACPI_BUFFER &&
                Buffer->Type != ACPI_PACKAGE) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            uint64_t Index;
            if (!AcpipExecuteInteger(State, &Index)) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            AcpipTarget Target;
            if (!AcpipExecuteTarget(State, &Target)) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            /* Pre-validate the index value (prevent buffer overflows). */
            switch (Buffer->Type) {
                case ACPI_STRING: {
                    if (Index > strlen(Buffer->String->Data)) {
                        AcpiRemoveReference(Buffer, 1);
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    if (Index >= Buffer->Buffer->Size) {
                        AcpiRemoveReference(Buffer, 1);
                        return 0;
                    }

                    break;
                }

                default: {
                    if (Index >= Buffer->Package->Size) {
                        AcpiRemoveReference(Buffer, 1);
                        return 0;
                    }

                    break;
                }
            }

            Value->Type = ACPI_INDEX;
            Value->References = 1;
            Value->Index.Source = Buffer;
            Value->Index.Index = Index;

            if (!AcpipStoreTarget(State, &Target, Value)) {
                AcpiRemoveReference(Buffer, 1);
                return 0;
            }

            break;
        }

        /* DefCondRefOf := CondRefOfOp SuperName Target */
        case 0x125B: {
            AcpipTarget SuperName;
            if (!AcpipExecuteSuperName(State, &SuperName, 1)) {
                return 0;
            }

            AcpipTarget Target;
            if (!AcpipExecuteTarget(State, &Target)) {
                return 0;
            }

            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = SuperName.Type == ACPI_TARGET_UNRESOLVED ? 0 : UINT64_MAX;

            if (SuperName.Type != ACPI_TARGET_UNRESOLVED) {
                AcpiValue Reference;
                memset(&Reference, 0, sizeof(AcpiValue));

                switch (SuperName.Type) {
                    case ACPI_TARGET_LOCAL:
                        Reference.Type = ACPI_LOCAL;
                        Reference.Integer = SuperName.Index;
                        break;

                    case ACPI_TARGET_ARG:
                        Reference.Type = ACPI_ARG;
                        Reference.Integer = SuperName.Index;
                        break;

                    case ACPI_TARGET_NAMED:
                        Reference.Type = ACPI_REFERENCE;
                        Reference.Reference = SuperName.Object;
                        AcpiCreateReference(&SuperName.Object->Value, NULL);
                        break;
                }

                if (!AcpipStoreTarget(State, &Target, &Reference)) {
                    return 0;
                }
            }

            break;
        }

        default:
            return -1;
    }

    return 1;
}
