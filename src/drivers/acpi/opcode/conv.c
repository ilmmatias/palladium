/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdlib.h>

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
            if (!AcpipExecuteOpcode(State, Value, 0)) {
                return 0;
            }

            AcpipTarget Target;
            if (!AcpipExecuteTarget(State, &Target)) {
                return 0;
            }

            if (!AcpipCastToBuffer(Value)) {
                return 0;
            }

            if (!AcpipStoreTarget(State, &Target, Value)) {
                return 0;
            }

            break;
        }

        /* DefToDecimalString := ToDecimalStringOp Operand Target
           DefToHexString := ToHexStringOp Operand Target */
        case 0x97:
        case 0x98: {
            if (!AcpipExecuteOpcode(State, Value, 0)) {
                return 0;
            }

            AcpipTarget Target;
            if (!AcpipExecuteTarget(State, &Target)) {
                return 0;
            }

            if (!AcpipCastToString(Value, 0, Opcode == 0x97)) {
                return 0;
            }

            if (!AcpipStoreTarget(State, &Target, Value)) {
                return 0;
            }

            break;
        }

        /* DefToInteger := ToIntegerOp Operand Target */
        case 0x99: {
            if (!AcpipExecuteOpcode(State, Value, 0)) {
                return 0;
            }

            AcpipTarget Target;
            if (!AcpipExecuteTarget(State, &Target)) {
                return 0;
            }

            uint64_t Integer;
            if (!AcpipCastToInteger(Value, &Integer, 1)) {
                return 0;
            }

            if (!AcpipStoreTarget(State, &Target, Value)) {
                return 0;
            }

            break;
        }

        default:
            return -1;
    }

    return 1;
}
