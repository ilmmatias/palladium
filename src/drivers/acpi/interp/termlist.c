/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes all operations within the current scope.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteTermList(AcpipState *State) {
    int Status = 0;

    while (1) {
        if (!State->Scope->RemainingLength) {
            /* Backtrack into the previous scope, or end if we're already in the top-most
               scope. */
            AcpipScope *Parent = State->Scope->Parent;
            if (!Parent) {
                Status = 1;
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
                    !AcpipCastToInteger(&PredicateValue, &Predicate, 1)) {
                    return 0;
                } else if (!Predicate) {
                    break;
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
            Status = 1;
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
