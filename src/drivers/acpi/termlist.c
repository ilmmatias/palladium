/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

AcpiValue *AcpipExecuteTermList(AcpipState *State) {
    while (1) {
        if (!State->Scope->RemainingLength) {
            /* Backtrack into the previous scope, or end if we're already in the top-most
               scope. */
            AcpipScope *Parent = State->Scope->Parent;
            if (!Parent) {
                break;
            }

            Parent->Code = State->Scope->Code;
            Parent->RemainingLength -= State->Scope->Length;

            /* Solve the predicate on while loops, repeating the loop iteration if required. */
            if (State->Scope->Predicate) {
                State->Scope->Code = State->Scope->Predicate;
                State->Scope->RemainingLength = State->Scope->PredicateBacktrack;

                uint64_t Predicate;
                if (!AcpipExecuteInteger(State, &Predicate)) {
                    return NULL;
                }

                if (Predicate) {
                    Parent->Code = State->Scope->Code;
                    Parent->RemainingLength = State->Scope->RemainingLength;
                }
            }

            free(State->Scope);
            State->Scope = Parent;

            continue;
        }

        if (!AcpipExecuteOpcode(State, NULL)) {
            return NULL;
        }
    }

    AcpiValue *Value = malloc(sizeof(AcpiValue));
    if (Value) {
        Value->Type = ACPI_INTEGER;
        Value->Integer = 0;
    }

    return Value;
}
