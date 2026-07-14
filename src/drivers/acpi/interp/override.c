/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <acpip.h>
#include <stddef.h>
#include <stdint.h>
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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
static bool ExecuteOsi(int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    if (ArgCount >= 1 && Arguments[0].Type == ACPI_STRING) {
        AcpipShowTraceMessage("request for \\_OSI, feature = %s\n", Arguments[0].String->Data);
    } else {
        AcpipShowDebugMessage("request for \\_OSI, no valid arguments\n");
    }

    /* Return TRUE for Darwin, otherwise, we'll probably fail to boot on Macs. */
    if (ArgCount >= 1 && Arguments[0].Type == ACPI_STRING &&
        (!strncmp(Arguments[0].String->Data, "Windows ", 8) ||
         !strcmp(Arguments[0].String->Data, "Darwin") ||
         !strcmp(Arguments[0].String->Data, "Module Device") ||
         !strcmp(Arguments[0].String->Data, "Processor Device") ||
         !strcmp(Arguments[0].String->Data, "3.0 Thermal Model") ||
         !strcmp(Arguments[0].String->Data, "Extended Address Space Descriptor") ||
         !strcmp(Arguments[0].String->Data, "3.0 _SCP Extensions") ||
         !strcmp(Arguments[0].String->Data, "Processor Aggregator Device"))) {
        Result->Type = ACPI_INTEGER;
        Result->References = 1;
        Result->Integer = UINT64_MAX;
        return true;
    }

    Result->Type = ACPI_INTEGER;
    Result->References = 1;
    Result->Integer = 0;
    return true;
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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
static bool ExecuteOs(int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    (void)ArgCount;
    (void)Arguments;

    AcpipShowTraceMessage("request for \\_OS\n");

    Result->Type = ACPI_STRING;
    Result->References = 1;
    Result->String = AcpipAllocateBlock(sizeof(AcpiString) + strlen("Microsoft Windows NT") + 1);
    if (Result->String) {
        Result->String->References = 1;
        strcpy(Result->String->Data, "Microsoft Windows NT");
    }

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
 *     true/false depending on success.
 *-----------------------------------------------------------------------------------------------*/
static bool ExecuteRev(int ArgCount, AcpiValue *Arguments, AcpiValue *Result) {
    (void)ArgCount;
    (void)Arguments;

    AcpipShowTraceMessage("request for \\_REV\n");

    Result->Type = ACPI_INTEGER;
    Result->References = 1;
    Result->Integer = ACPI_REVISION;

    return true;
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

    AcpiObject *Objects = AcpipAllocateZeroBlock(OVERRIDE_ITEMS, sizeof(AcpiObject));
    if (!Objects) {
        AcpipShowErrorMessage(
            ACPI_REASON_OUT_OF_MEMORY, "could not allocate the predefined methods\n");
    }

    AcpiChildren *Children = AcpipAllocateZeroBlock(OVERRIDE_ITEMS, sizeof(AcpiChildren));
    if (!Children) {
        AcpipShowErrorMessage(
            ACPI_REASON_OUT_OF_MEMORY, "could not allocate the predefined methods\n");
    }

    for (int i = 0; i < OVERRIDE_ITEMS; i++) {
        memcpy(Objects[i].Name, Names[i], 4);
        Objects[i].Value.Type = ACPI_METHOD;
        Objects[i].Value.References = 1;
        Objects[i].Value.Children = &Children[i];
        Children[i].References = 1;
        Children[i].Objects = NULL;
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
    AcpiObject *Parent = AcpipObjectTree->Value.Children->Objects;
    while (Parent->Next != NULL) {
        Parent = Parent->Next;
    }

    Parent->Next = Objects;
}
