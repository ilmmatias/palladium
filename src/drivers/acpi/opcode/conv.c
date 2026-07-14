/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <acpip.h>
#include <stdint.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Conversion Operation.
 *     We implement all conversion/to-X related ops from the `20.2.5.4. Expression Opcodes Encoding`
 *     section of the AML spec here.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *     Value - Output; Where to store the resulting Value.
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a conversion op", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteConvOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value) {
    switch (Opcode) {
        /* DefToBuffer := ToBufferOp Operand Target */
        case 0x96: {
            memcpy(Value, &State->Opcode->FixedArguments[0].TermArg, sizeof(AcpiValue));
            AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

            if (!AcpipCastToBuffer(Value)) {
                AcpiRemoveReference(Target, false);
                return 0;
            }

            if (!AcpipStoreTarget(State, Target, Value)) {
                AcpiRemoveReference(Target, false);
                return 0;
            }

            AcpiRemoveReference(Target, false);
            break;
        }

        /* DefToDecimalString := ToDecimalStringOp Operand Target
           DefToHexString := ToHexStringOp Operand Target */
        case 0x97:
        case 0x98: {
            memcpy(Value, &State->Opcode->FixedArguments[0].TermArg, sizeof(AcpiValue));
            AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

            if (!AcpipCastToString(Value, false, Opcode == 0x97)) {
                AcpiRemoveReference(Target, false);
                return 0;
            }

            if (!AcpipStoreTarget(State, Target, Value)) {
                AcpiRemoveReference(Target, false);
                return 0;
            }

            AcpiRemoveReference(Target, false);
            break;
        }

        /* DefToInteger := ToIntegerOp Operand Target */
        case 0x99: {
            AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            if (!AcpipCastToInteger(
                    &State->Opcode->FixedArguments[0].TermArg, &Value->Integer, true)) {
                AcpiRemoveReference(Target, false);
                return 0;
            }

            if (!AcpipStoreTarget(State, Target, Value)) {
                AcpiRemoveReference(Target, false);
                return 0;
            }

            AcpiRemoveReference(Target, false);
            break;
        }

        /* DefFromBCD := FromBCDOp BCDValue Target */
        case 0x285B: {
            uint64_t BCDValue = State->Opcode->FixedArguments[0].TermArg.Integer;
            AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

            /* BCD packs each digit into a nibble, or extracting for forming numbers is as easy as
             * shifting the nibbles around. */
            uint64_t Result = 0;
            uint64_t Multiplier = 1;
            for (int i = 0; i < 16; i++) {
                uint8_t Nibble = (BCDValue >> (i * 4)) & 0x0F;
                if (Nibble > 9) {
                    AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, false);
                    AcpiRemoveReference(Target, false);
                    return 0;
                }

                Result += Nibble * Multiplier;
                Multiplier *= 10;
            }

            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = Result;

            if (!AcpipStoreTarget(State, Target, Value)) {
                AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, false);
                AcpiRemoveReference(Target, false);
                return 0;
            }

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, false);
            AcpiRemoveReference(Target, false);
            break;
        }

        /* DefToBCD := ToBCDOp Operand Target */
        case 0x295B: {
            uint64_t Operand = State->Opcode->FixedArguments[0].TermArg.Integer;
            AcpiValue *Target = &State->Opcode->FixedArguments[1].TermArg;

            uint64_t Result = 0;
            uint64_t Temp = Operand;
            for (int i = 0; i < 16 && Temp > 0; i++) {
                Result |= (Temp % 10) << (i * 4);
                Temp /= 10;
            }

            if (Temp > 0) {
                AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, false);
                AcpiRemoveReference(Target, false);
                return 0;
            }

            Value->Type = ACPI_INTEGER;
            Value->References = 1;
            Value->Integer = Result;

            if (!AcpipStoreTarget(State, Target, Value)) {
                AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, false);
                AcpiRemoveReference(Target, false);
                return 0;
            }

            AcpiRemoveReference(&State->Opcode->FixedArguments[0].TermArg, false);
            AcpiRemoveReference(Target, false);
            break;
        }

        default:
            return -1;
    }

    return 1;
}
