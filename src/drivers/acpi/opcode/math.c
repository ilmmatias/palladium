/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Mathematical Operation.
 *     We implement all math-related ops from the `20.2.5.4. Expression Opcodes Encoding` section
 *     of the AML spec here.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *     Value - Output; Where to store the resulting Value.
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a math op", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteMathOpcode(AcpipState *State, uint16_t Opcode, AcpiValue *Value) {
    switch (Opcode) {
        /* Binary operations with a target (all follow the format: Op Operand Operand Target). */
        case 0x72:
        case 0x74:
        case 0x77:
        case 0x79:
        case 0x7A:
        case 0x7B:
        case 0x7C:
        case 0x7D:
        case 0x7E:
        case 0x7F:
        case 0x85: {
            uint64_t Left;
            if (!AcpipExecuteInteger(State, &Left)) {
                return 0;
            }

            uint64_t Right;
            if (!AcpipExecuteInteger(State, &Right)) {
                return 0;
            }

            AcpipTarget *Target = AcpipExecuteTarget(State);
            if (!Target) {
                return 0;
            }

            Value->Type = ACPI_INTEGER;
            switch (Opcode) {
                case 0x72:
                    Value->Integer = Left + Right;
                    break;
                case 0x74:
                    Value->Integer = Left - Right;
                    break;
                case 0x77:
                    Value->Integer = Left * Right;
                    break;
                case 0x79:
                    Value->Integer = Left << Right;
                    break;
                case 0x7A:
                    Value->Integer = Left >> Right;
                    break;
                case 0x7B:
                    Value->Integer = Left & Right;
                    break;
                case 0x7C:
                    Value->Integer = ~(Left & Right);
                    break;
                case 0x7D:
                    Value->Integer = Left | Right;
                    break;
                case 0x7E:
                    Value->Integer = ~(Left | Right);
                    break;
                case 0x7F:
                    Value->Integer = Left ^ Right;
                    break;
                case 0x85:
                    Value->Integer = Left % Right;
                    break;
            }

            int Status = AcpipStoreTarget(State, Target, Value);
            free(Target);

            if (!Status) {
                return 0;
            }

            break;
        }

        /* Unary operations (all follow the format: Op Target, where Target is always a
           SuperName). */
        case 0x75:
        case 0x76: {
            AcpipTarget *Target = AcpipExecuteSuperName(State);
            if (!Target) {
                return 0;
            }

            uint64_t TargetInteger;
            AcpiValue TargetValue;
            if (!AcpipReadTarget(State, Target, &TargetValue)) {
                free(Target);
                return 0;
            } else if (!AcpipCastToInteger(&TargetValue, &TargetInteger)) {
                free(Target);
                return 0;
            }

            Value->Type = ACPI_INTEGER;
            switch (Opcode) {
                case 0x75:
                    Value->Integer = TargetInteger + 1;
                    break;
                case 0x76:
                    Value->Integer = TargetInteger - 1;
                    break;
            }

            int Status = AcpipStoreTarget(State, Target, Value);
            free(Target);

            if (!Status) {
                return 0;
            }

            break;
        }

        /* Binary operations without a target (all follow the format: Op Operand Operand). */
        case 0x90:
        case 0x91:
        case 0x93:
        case 0x94:
        case 0x95: {
            uint64_t Left;
            if (!AcpipExecuteInteger(State, &Left)) {
                return 0;
            }

            uint64_t Right;
            if (!AcpipExecuteInteger(State, &Right)) {
                return 0;
            }

            Value->Type = ACPI_INTEGER;
            switch (Opcode) {
                case 0x90:
                    Value->Integer = Left && Right;
                    break;
                case 0x91:
                    Value->Integer = Left || Right;
                    break;
                case 0x93:
                    Value->Integer = Left == Right;
                    break;
                case 0x94:
                    Value->Integer = Left > Right;
                    break;
                case 0x95:
                    Value->Integer = Left < Right;
                    break;
            }

            break;
        }

        /* DefLNot := LnotOp Operand */
        case 0x92: {
            uint64_t Operand;
            if (!AcpipExecuteInteger(State, &Operand)) {
                return 0;
            }

            Value->Type = ACPI_INTEGER;
            Value->Integer = !Operand;

            break;
        }

        default:
            return -1;
    }

    return 1;
}
