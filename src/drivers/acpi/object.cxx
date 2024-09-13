/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <object.hxx>
#include <os.hxx>

static SList<AutoPtr<AcpipObject>> Entries("Acpi");

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds all builtin scopes to our global scope table.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipInitializeBuiltin(void) {
    const char *BuiltinScopes[]{"_PR_", "_SB_", "_SI_", "_TZ_"};

    for (auto &ScopeName : BuiltinScopes) {
        AutoPtr<AcpipObject> Scope("Acpi");
        memcpy(Scope->Name, ScopeName, 4);
        Scope->Value.Type = AcpipValueType::Scope;
        Entries.Push(Scope.Move());
    }
}
