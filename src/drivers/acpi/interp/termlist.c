/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <acpip.h>
#include <stddef.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes all operations within the current scope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *
 * RETURN VALUE:
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
bool AcpipExecuteTermList(AcpipState *State) {
    bool Status = false;

    while (true) {
        if (!State->Scope->RemainingLength) {
            /* Backtrack into the previous scope, or end if we're already in the top-most
               scope. */
            AcpipScope *Parent = State->Scope->Parent;
            if (!Parent) {
                Status = true;
                break;
            }

            /* Solve the predicate on while loops, repeating the loop iteration if required. */
            if (State->Scope->Predicate) {
                State->Scope->Code = State->Scope->Predicate;
                State->Scope->RemainingLength = State->Scope->PredicateBacktrack;

                uint64_t Predicate;
                AcpiValue PredicateValue;
                if (!AcpipPrepareExecuteOpcode(State) ||
                    !AcpipExecuteOpcode(State, &PredicateValue) ||
                    !AcpipCastToInteger(&PredicateValue, &Predicate, true)) {
                    return false;
                }

                if (Predicate) {
                    State->Scope->RemainingLength = State->Scope->Length;
                    continue;
                }

                State->Scope->Code += State->Scope->Length;
            }

            Parent->Code = State->Scope->Code;
            Parent->RemainingLength -= State->Scope->Length;

            AcpipFreeBlock(State->Scope);
            State->Scope = Parent;

            continue;
        }

        if (!AcpipPrepareExecuteOpcode(State) || !AcpipExecuteOpcode(State, NULL)) {
            break;
        }

        if (State->HasReturned) {
            Status = true;
            break;
        }
    }

    /* Base of the scope stack should be a stack-allocated scope (instead of heap-allocated), so
       we can't free that; But we do need to free anything left in case of an early exit (because
       of a Return). */
    while (State->Scope->Parent) {
        AcpipScope *Parent = State->Scope->Parent;
        AcpipFreeBlock(State->Scope);
        State->Scope = Parent;
    }

    /* State should also be stack-allocated (and it contains the return value anyways), so we
       can't free it either. */
    return Status;
}
