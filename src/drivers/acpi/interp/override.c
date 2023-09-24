/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <ke.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern AcpiObject *AcpipObjectTree;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Implementation of \_OSI; We always identify ourselves as Windows, as some/lots of BIOSes
 *     will probably break/disable a lot of stuff if we don't.
 *
 * PARAMETERS:
 *     ArgCount - Argument count.
 *     Arguments - Array of arguments.
 *     Result - Output; Where to store the result value, if there's any.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ExecuteOsi(int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    /* Return TRUE for Darwin, otherwise, we'll probably fail to boot on Macs. */
    if (ArgCount >= 1 && Arguments[0].Type == ACPI_STRING &&
        (!strncmp(Arguments[0].String, "Windows ", 8) || !strcmp(Arguments[0].String, "Darwin") ||
         !strcmp(Arguments[0].String, "Module Device") ||
         !strcmp(Arguments[0].String, "Processor Device") ||
         !strcmp(Arguments[0].String, "3.0 Thermal Model") ||
         !strcmp(Arguments[0].String, "Extended Address Space Descriptor") ||
         !strcmp(Arguments[0].String, "3.0 _SCP Extensions") ||
         !strcmp(Arguments[0].String, "Processor Aggregator Device"))) {
        Result->Type = ACPI_INTEGER;
        Result->Integer = UINT64_MAX;
        return 1;
    }

    Result->Type = ACPI_INTEGER;
    Result->References = 1;
    Result->Integer = 0;
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Implementation of \_OS, always returning that we're Windows NT.
 *
 * PARAMETERS:
 *     ArgCount - Argument count.
 *     Arguments - Array of arguments.
 *     Result - Output; Where to store the result value, if there's any.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ExecuteOs(int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    (void)ArgCount;
    (void)Arguments;
    Result->Type = ACPI_STRING;
    Result->References = 1;
    Result->String = strdup("Microsoft Windows NT");
    return Result->String != NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Implementation of \_REV; ACPI v2, there are BIOSes that break with any other value!
 *
 * PARAMETERS:
 *     ArgCount - Argument count.
 *     Arguments - Array of arguments.
 *     Result - Output; Where to store the result value, if there's any.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static int ExecuteRev(int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    (void)ArgCount;
    (void)Arguments;
    Result->Type = ACPI_INTEGER;
    Result->References = 1;
    Result->Integer = 2;
    return 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the methods that the AML code expects us to handle (instead of
 *     them/an external table).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipPopulateOverride(void) {
#define OVERRIDE_ITEMS 3
    static const char Names[OVERRIDE_ITEMS][5] = {
        "_OSI",
        "_OS_",
        "_REV",
    };

    static AcpiOverrideMethod Methods[OVERRIDE_ITEMS] = {
        ExecuteOsi,
        ExecuteOs,
        ExecuteRev,
    };

    static uint8_t MethodFlags[OVERRIDE_ITEMS] = {
        1,
        0,
        0,
    };

    AcpiObject *Objects = calloc(OVERRIDE_ITEMS, sizeof(AcpiObject));
    if (!Objects) {
        KeFatalError(KE_EARLY_MEMORY_FAILURE);
    }

    for (int i = 0; i < OVERRIDE_ITEMS; i++) {
        memcpy(Objects[i].Name, Names[i], 4);
        Objects[i].Value.Type = ACPI_METHOD;
        Objects[i].Value.References = 1;
        Objects[i].Value.Objects = NULL;
        Objects[i].Value.Method.Override = Methods[i];
        Objects[i].Value.Method.Start = NULL;
        Objects[i].Value.Method.Size = 0;
        Objects[i].Value.Method.Flags = MethodFlags[i];
        Objects[i].Next = NULL;
        Objects[i].Parent = AcpipObjectTree;

        if (i != OVERRIDE_ITEMS - 1) {
            Objects[i].Next = &Objects[i + 1];
        }
    }

    /* We'll be appending all items to the last entry in the root scope. */
    AcpiObject *Parent = AcpipObjectTree->Value.Objects;
    while (Parent->Next != NULL) {
        Parent = Parent->Next;
    }

    Parent->Next = Objects;
}
