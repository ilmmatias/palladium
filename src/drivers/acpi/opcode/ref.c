/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdio.h>
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
            AcpiValue *Reference = &State->Opcode->FixedArguments[0].TermArg;

            switch (Reference->Type) {
                case ACPI_INDEX: {
                    AcpiValue *Source = Reference->BufferField.Source;
                    uint64_t Index = Reference->BufferField.Index;

                    if (Source->Type == ACPI_PACKAGE) {
                        if (Source->Package->Data[Index].Type) {
                            AcpiCreateReference(&Source->Package->Data[Index].Value, Value);
                        } else {
                            /* For NameStrings, we need to resolve them + create a reference to
                               their contents; by now, the object they point to should exist, so
                               we're erroring out if they don't. */
                            AcpiName Name;
                            memcpy(&Name, &Source->Package->Data[Index].Name, sizeof(AcpiName));

                            AcpiObject *Object = AcpipResolveObject(&Name);
                            if (!Object) {
                                AcpiRemoveReference(Reference, 0);
                                return 0;
                            }

                            AcpiCreateReference(&Object->Value, Value);
                        }
                    } else {
                        Value->Type = ACPI_INTEGER;
                        Value->References = 1;
                        Value->Integer = Source->Type == ACPI_STRING ? Source->String->Data[Index]
                                                                     : Source->Buffer->Data[Index];
                    }

                    break;
                }

                case ACPI_LOCAL:
                    AcpiCreateReference(&State->Locals[Reference->Integer], Value);
                    break;

                case ACPI_ARG:
                    AcpiCreateReference(&State->Arguments[Reference->Integer], Value);
                    break;

                case ACPI_REFERENCE:
                    AcpiCreateReference(&Reference->Reference->Value, Value);
                    break;

                default:
                    AcpiRemoveReference(Reference, 0);
                    return 0;
            }

            AcpiRemoveReference(Reference, 0);
            break;
        }

        /* DefIndex := IndexOp BuffPkgStrObj IndexValue Target */
        case 0x88: {
            AcpiValue *Target = &State->Opcode->FixedArguments[2].TermArg;

            /* BufferField takes a reference to an AcpiValue (that needs to live for long
               enough), so allocate some memory for it. */
            AcpiValue *Buffer = AcpipAllocateBlock(sizeof(AcpiValue));
            AcpiCreateReference(&State->Opcode->FixedArguments[0].TermArg, Buffer);
            if (Buffer->Type != ACPI_STRING && Buffer->Type != ACPI_BUFFER &&
                Buffer->Type != ACPI_PACKAGE) {
                AcpiRemoveReference(Buffer, 1);
                AcpiRemoveReference(Target, 0);
                return 0;
            }

            /* Pre-validate the index value (prevent buffer overflows). */
            uint64_t Index = State->Opcode->FixedArguments[1].TermArg.Integer;
            switch (Buffer->Type) {
                case ACPI_STRING: {
                    if (Index > strlen(Buffer->String->Data)) {
                        AcpiRemoveReference(Buffer, 1);
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }

                    break;
                }

                case ACPI_BUFFER: {
                    if (Index >= Buffer->Buffer->Size) {
                        AcpiRemoveReference(Buffer, 1);
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }

                    break;
                }

                default: {
                    if (Index >= Buffer->Package->Size) {
                        AcpiRemoveReference(Buffer, 1);
                        AcpiRemoveReference(Target, 0);
                        return 0;
                    }

                    break;
                }
            }

            Value->Type = ACPI_INDEX;
            Value->References = 1;
            Value->BufferField.Source = Buffer;
            Value->BufferField.Index = Index;

            if (!AcpipStoreTarget(State, Target, Value)) {
                AcpiRemoveReference(Buffer, 1);
                AcpiRemoveReference(Target, 0);
                return 0;
            }

            AcpiRemoveReference(Target, 0);
            break;
        }

        /* DefCondRefOf := CondRefOfOp SuperName Target */
        case 0x125B: {
            AcpiValue *SuperName = &State->Opcode->FixedArguments[0].TermArg;
            AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = SuperName->Type == ACPI_EMPTY ? 0 : UINT64_MAX;

            if (SuperName->Type != ACPI_EMPTY && !AcpipStoreTarget(State, Target, SuperName)) {
                AcpiRemoveReference(Target, 0);
                return 0;
            }

            AcpiRemoveReference(Target, 0);
            break;
        }

        default:
            return -1;
    }

    return 1;
}
