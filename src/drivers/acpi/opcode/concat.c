/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Concatenation Operation.
 *     We implement all concat related ops from the `20.2.5.4. Expression Opcodes Encoding`
 *     section of the AML spec here.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *     Value - Output; Where to store the resulting Value.
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a concat op", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteConcatOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value) {
    switch (Opcode) {
        /* DefConcat := ConcatOp Data Data Target */
        case 0x73: {
            AcpiValue *Left = &State->Opcode->FixedArguments[0].TermArg;
            AcpiValue *Right = &State->Opcode->FixedArguments[1].TermArg;

            switch (Left->Type) {
                /* Read it as two integers, and append them into a buffer. */
                case ACPI_INTEGER: {
                    uint64_t LeftValue = Left->Integer;
                    uint64_t RightValue;
                    int Result = AcpipCastToInteger(Right, &RightValue, 1);
                    if (!Result) {
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);
                        return 0;
                    }

                    Value->Type = ACPI_BUFFER;
                    Value->References = 1;
                    Value->Buffer = malloc(sizeof(AcpiBuffer) + 16);
                    if (!Value->Buffer) {
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);
                        return 0;
                    }

                    Value->Buffer->References = 1;
                    Value->Buffer->Size = 16;
                    *((uint64_t *)Value->Buffer->Data) = LeftValue;
                    *((uint64_t *)(Value->Buffer->Data + 8)) = RightValue;

                    break;
                }

                /* Read it as two buffers, and append them into another buffer. */
                case ACPI_BUFFER: {
                    if (!AcpipCastToBuffer(Right)) {
                        AcpiRemoveReference(Left, 0);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);
                        return 0;
                    }

                    Value->Type = ACPI_BUFFER;
                    Value->References = 1;
                    Value->Buffer =
                        malloc(sizeof(AcpiBuffer) + Left->Buffer->Size + Right->Buffer->Size);
                    if (!Value->Buffer) {
                        AcpiRemoveReference(Left, 0);
                        AcpiRemoveReference(Right, 0);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);
                        return 0;
                    }

                    Value->Buffer->References = 1;
                    Value->Buffer->Size = Left->Buffer->Size + Right->Buffer->Size;
                    memcpy(Value->Buffer->Data, Left->Buffer->Data, Left->Buffer->Size);
                    memcpy(
                        Value->Buffer->Data + Left->Buffer->Size,
                        Right->Buffer->Data,
                        Right->Buffer->Size);

                    break;
                }

                /* Convert both sides into strings, and append them into a single string. */
                default: {
                    if (!AcpipCastToString(Left, 1, 0) || !AcpipCastToString(Right, 1, 0)) {
                        AcpiRemoveReference(Left, 0);
                        AcpiRemoveReference(Right, 0);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);
                        return 0;
                    }

                    Value->Type = ACPI_STRING;
                    Value->References = 1;
                    Value->String = malloc(
                        sizeof(AcpiString) + strlen(Left->String->Data) +
                        strlen(Right->String->Data) + 1);
                    if (!Value->String) {
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);
                        return 0;
                    }

                    Value->String->References = 1;
                    strcpy(Value->String->Data, Left->String->Data);
                    strcat(Value->String->Data, Right->String->Data);

                    break;
                }
            }

            int Status = AcpipStoreTarget(State, &State->Opcode->FixedArguments[2].TermArg, Value);
            AcpiRemoveReference(Left, 0);
            AcpiRemoveReference(Right, 0);
            AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, 0);

            if (!Status) {
                return 0;
            }

            break;
        }

        default:
            return -1;
    }

    return 1;
}
