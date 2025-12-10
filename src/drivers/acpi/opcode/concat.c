/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
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
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                        return 0;
                    }

                    Value->Type = ACPI_BUFFER;
                    Value->References = 1;
                    Value->Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + 16);
                    if (!Value->Buffer) {
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
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
                        AcpiRemoveReference(Left, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                        return 0;
                    }

                    Value->Type = ACPI_BUFFER;
                    Value->References = 1;
                    Value->Buffer = AcpipAllocateBlock(
                        sizeof(AcpiBuffer) + Left->Buffer->Size + Right->Buffer->Size);
                    if (!Value->Buffer) {
                        AcpiRemoveReference(Left, false);
                        AcpiRemoveReference(Right, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
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
                        AcpiRemoveReference(Left, false);
                        AcpiRemoveReference(Right, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                        return 0;
                    }

                    Value->Type = ACPI_STRING;
                    Value->References = 1;
                    Value->String = AcpipAllocateBlock(
                        sizeof(AcpiString) + strlen(Left->String->Data) +
                        strlen(Right->String->Data) + 1);
                    if (!Value->String) {
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                        return 0;
                    }

                    Value->String->References = 1;
                    strcpy(Value->String->Data, Left->String->Data);
                    strcat(Value->String->Data, Right->String->Data);

                    break;
                }
            }

            int Status = AcpipStoreTarget(State, &State->Opcode->FixedArguments[2].TermArg, Value);
            AcpiRemoveReference(Left, false);
            AcpiRemoveReference(Right, false);
            AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);

            if (!Status) {
                return 0;
            }

            break;
        }

        /* DefMid := MidOp Source Index Length Result */
        case 0x9E: {
            AcpiValue *Source = &State->Opcode->FixedArguments[0].TermArg;
            uint64_t Index = State->Opcode->FixedArguments[1].TermArg.Integer;
            uint64_t Length = State->Opcode->FixedArguments[2].TermArg.Integer;
            AcpiValue *Target = &State->Opcode->FixedArguments[3].TermArg;

            switch (Source->Type) {
                /* Extract a portion of a buffer. */
                case ACPI_BUFFER: {
                    uint64_t SourceLen = Source->Buffer->Size;
                    if (Index >= SourceLen) {
                        Index = SourceLen;
                        Length = 0;
                    } else if (Index + Length > SourceLen) {
                        Length = SourceLen - Index;
                    }

                    Value->Type = ACPI_BUFFER;
                    Value->References = 1;
                    Value->Buffer = AcpipAllocateBlock(sizeof(AcpiBuffer) + Length);
                    if (!Value->Buffer) {
                        AcpiRemoveReference(Source, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[1].TermArg, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                        AcpiRemoveReference(Target, false);
                        return 0;
                    }

                    Value->Buffer->References = 1;
                    Value->Buffer->Size = Length;
                    if (Length > 0) {
                        memcpy(Value->Buffer->Data, Source->Buffer->Data + Index, Length);
                    }

                    break;
                }

                /* Extract a portion of a string. */
                case ACPI_STRING: {
                    uint64_t SourceLen = strlen(Source->String->Data);
                    if (Index >= SourceLen) {
                        Index = SourceLen;
                        Length = 0;
                    } else if (Index + Length > SourceLen) {
                        Length = SourceLen - Index;
                    }

                    Value->Type = ACPI_STRING;
                    Value->References = 1;
                    Value->String = AcpipAllocateBlock(sizeof(AcpiString) + Length + 1);
                    if (!Value->String) {
                        AcpiRemoveReference(Source, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[1].TermArg, false);
                        AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                        AcpiRemoveReference(Target, false);
                        return 0;
                    }

                    if (Length > 0) {
                        memcpy(Value->String->Data, Source->String->Data + Index, Length);
                    }

                    Value->String->Data[Length] = '\0';
                    Value->String->References = 1;
                    break;
                }

                default: {
                    AcpiRemoveReference(Source, false);
                    AcpiRemoveReference(&State->Opcode->FixedArguments[1].TermArg, false);
                    AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
                    AcpiRemoveReference(Target, false);
                    return 0;
                }
            }

            int Status = AcpipStoreTarget(State, Target, Value);
            AcpiRemoveReference(Source, false);
            AcpiRemoveReference(&State->Opcode->FixedArguments[1].TermArg, false);
            AcpiRemoveReference(&State->Opcode->FixedArguments[2].TermArg, false);
            AcpiRemoveReference(Target, false);

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
