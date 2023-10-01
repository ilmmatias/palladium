/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries the execute the given opcode as a Namespace Modifier opcode.
 *
 * PARAMETERS:
 *     State - AML state containing the current scope.
 *     Opcode - Opcode to be executed; The high bits contain the extended opcode number (when the
 *              lower opcode is 0x5B).
 *
 * RETURN VALUE:
 *     Positive number on success, negative on "this isn't a namespace modifier object", 0 on
 *     failure.
 *-----------------------------------------------------------------------------------------------*/
int AcpipExecuteNsModOpcode(AcpipState *State, uint16_t Opcode) {
    uint32_t Start = State->Opcode->Start;

    switch (Opcode) {
        /* DefAlias := AliasOp NameString NameString */
        case 0x06: {
            AcpiObject *SourceObject = AcpipResolveObject(&State->Opcode->FixedArguments[0].Name);
            if (!SourceObject) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_ALIAS;
            Value.References = 1;
            Value.Alias = SourceObject;

            AcpiObject *Object = AcpipCreateObject(&State->Opcode->FixedArguments[1].Name, &Value);
            if (!Object) {
                return 0;
            }

            break;
        }

        /* DefName := NameOp NameString DataRefObject */
        case 0x08: {
            AcpiObject *Object = AcpipCreateObject(
                &State->Opcode->FixedArguments[0].Name, &State->Opcode->FixedArguments[1].TermArg);
            if (!Object) {
                return 0;
            }

            break;
        }

        /* DefScope := ScopeOp PkgLength NameString TermList */
        case 0x10: {
            uint32_t Length = State->Opcode->PkgLength;
            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_SCOPE;
            Value.References = 1;
            Value.Children = malloc(sizeof(AcpiChildren));
            if (!Value.Children) {
                return 0;
            }

            Value.Children->References = 1;
            Value.Children->Objects = NULL;

            AcpiObject *Object = AcpipCreateObject(&State->Opcode->FixedArguments[0].Name, &Value);
            if (!Object) {
                return 0;
            }

            AcpipScope *Scope = AcpipEnterScope(State, Object, Length - LengthSoFar);
            if (!Scope) {
                return 0;
            }

            State->Scope = Scope;
            break;
        }

        default:
            return -1;
    }

    return 1;
}
