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
    uint32_t Start = State->Scope->RemainingLength;

    switch (Opcode) {
        /* DefAlias := AliasOp NameString NameString */
        case 0x06: {
            AcpipName SourceName;
            if (!AcpipReadName(State, &SourceName)) {
                return 0;
            }

            AcpiObject *SourceObject = AcpipResolveObject(&SourceName);
            if (!SourceObject) {
                return 0;
            }

            AcpipName AliasName;
            if (!AcpipReadName(State, &AliasName)) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_ALIAS;
            Value.References = 1;
            Value.Alias = SourceObject;

            AcpiObject *Object = AcpipCreateObject(&AliasName, &Value);
            if (!Object) {
                return 0;
            }

            break;
        }

        /* DefName := NameOp NameString DataRefObject */
        case 0x08: {
            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

            AcpiValue DataRefObject;
            if (!AcpipExecuteOpcode(State, &DataRefObject, 1)) {
                return 0;
            }

            AcpiObject *Object = AcpipCreateObject(&Name, &DataRefObject);
            if (!Object) {
                return 0;
            }

            break;
        }

        /* DefScope := ScopeOp PkgLength NameString TermList */
        case 0x10: {
            uint32_t Length;
            if (!AcpipReadPkgLength(State, &Length)) {
                return 0;
            }

            AcpipName Name;
            if (!AcpipReadName(State, &Name)) {
                return 0;
            }

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

            AcpiObject *Object = AcpipCreateObject(&Name, &Value);
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
