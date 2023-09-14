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
            AcpipName *SourceName = AcpipReadName(State);
            if (!SourceName) {
                return 0;
            }

            AcpiObject *SourceObject = AcpipResolveObject(SourceName);
            if (!SourceObject) {
                free(SourceName);
                return 0;
            }

            AcpipName *AliasName = AcpipReadName(State);
            if (!AliasName) {
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_ALIAS;
            Value.Alias = SourceObject;

            AcpiObject *Object = AcpipCreateObject(AliasName, &Value);
            if (!Object) {
                free(AliasName);
                return 0;
            }

            break;
        }

        /* DefName := NameOp NameString DataRefObject */
        case 0x08: {
            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            AcpiValue DataRefObject;
            if (!AcpipExecuteOpcode(State, &DataRefObject)) {
                free(Name);
                return 0;
            }

            AcpiObject *Object = AcpipCreateObject(Name, &DataRefObject);
            if (!Object) {
                free(Name);
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

            AcpipName *Name = AcpipReadName(State);
            if (!Name) {
                return 0;
            }

            uint32_t LengthSoFar = Start - State->Scope->RemainingLength;
            if (LengthSoFar > Length || Length - LengthSoFar > State->Scope->RemainingLength) {
                free(Name);
                return 0;
            }

            AcpiValue Value;
            Value.Type = ACPI_SCOPE;
            Value.Objects = NULL;

            AcpiObject *Object = AcpipCreateObject(Name, &Value);
            if (!Object) {
                free(Name);
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
