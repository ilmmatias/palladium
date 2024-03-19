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
    AutoPtr<AcpipObject> Scope("Acpi");
    memcpy(Scope->Name, "_GPE", 4);
    Scope->Value.Type = AcpipValueType::Scope;
    Entries.Push(Scope.Move());

    Scope = AutoPtr<AcpipObject>("Acpi");
    memcpy(Scope->Name, "_PR_", 4);
    Scope->Value.Type = AcpipValueType::Scope;
    Entries.Push(Scope.Move());

    Scope = AutoPtr<AcpipObject>("Acpi");
    memcpy(Scope->Name, "_SB_", 4);
    Scope->Value.Type = AcpipValueType::Scope;
    Entries.Push(Scope.Move());

    Scope = AutoPtr<AcpipObject>("Acpi");
    memcpy(Scope->Name, "_SI_", 4);
    Scope->Value.Type = AcpipValueType::Scope;
    Entries.Push(Scope.Move());

    Scope = AutoPtr<AcpipObject>("Acpi");
    memcpy(Scope->Name, "_TZ_", 4);
    Scope->Value.Type = AcpipValueType::Scope;
    Entries.Push(Scope.Move());
}
