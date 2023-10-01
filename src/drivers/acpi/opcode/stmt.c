/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Statement opcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a statement", 0 on failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteStmtOpcode(AcpipState *State, uint16_t Opcode) {
    const uint8_t *StartCode = State->Opcode->StartCode;
    uint32_t Start = State->Opcode->Start;

    switch (Opcode) {
        /* DefIfElse := IfOp PkgLength Predicate TermList DefElse */
        case 0xA0: {
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            if (State->Opcode->FixedArguments[0].TermArg.Integer) {
                AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
                if (!Scope) {
                    return 0;
                }

                State->Scope = Scope;
                break;
            }

            State->Scope->Code += Length - LengthSoFar;
            State->Scope->RemainingLength -= Length - LengthSoFar;

            /* DefElse only really matter for If(false), and we ignore it anywhere else;
               Here, we try reading up the code (and entering the else scope) after
               If(false). */
            if (!State->Scope->RemainingLength || *State->Scope->Code != 0xA1) {
                break;
            }

            State->Scope->Code++;
            State->Scope->RemainingLength--;
            Start = State->Scope->RemainingLength;

            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpipScope *Scope = AcpipEnterIf(State, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefElse := ElseOp PkgLength TermList */
        case 0xA1: {
            uint32_t Length = State->Opcode->PkgLength;
            if (Length > Start) {
                return 0;
            }

            State->Scope->Code = StartCode + Length;
            State->Scope->RemainingLength = Start - Length;
            break;
        }

        /* DefWhile := WhileOp PkgLength Predicate TermList */
        case 0xA2: {
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            const uint8_t *Predicate = State->Opcode->Predicate;
            uint32_t PredicateBacktrack = State->Opcode->PredicateBacktrack;
            if (!State->Opcode->FixedArguments[0].TermArg.Integer) {
                State->Scope->Code += Length - LengthSoFar;
                State->Scope->RemainingLength -= Length - LengthSoFar;
                break;
            }

            AcpipScope *Scope =
                AcpipEnterWhile(State, Predicate, PredicateBacktrack, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        /* DefNoop
           DefBreakPoint (functionally equal to DefNoop for us) */
        case 0xA3:
        case 0xCC:
            break;

        /* DefReturn ArgObject */
        case 0xA4: {
            memcpy(
                &State->ReturnValue, &State->Opcode->FixedArguments[0].TermArg, sizeof(AcpiValue));
            State->HasReturned = 1;
            break;
        }

        /* DefBreak */
        case 0xA5: {
            if (State->Scope->Predicate) {
                /* Leave termlist.c to handle going into the parent scope. */
                State->Scope->Code += State->Scope->RemainingLength;
                State->Scope->RemainingLength = 0;
                State->Scope->Predicate = NULL;
            }

            break;
        }

        /* ContinueOp */
        case 0x9F: {
            if (State->Scope->Predicate) {
                State->Scope->Code += State->Scope->RemainingLength;
                State->Scope->RemainingLength = 0;
            }

            break;
        }

        default:
            return -1;
    }

    return 1;
}
