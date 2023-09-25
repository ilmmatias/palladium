/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdarg.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes all child devices inside the given scope.
 *     We follow the ACPI spec, by executing STA->INI->REG, and then recursing on the device.
 *
 * PARAMETERS:
 *     Root - Scope to search for more devices.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void InitializeChildren(AcpiObject *Root) {
    for (AcpiObject *Device = Root->Value.Children->Objects; Device != NULL;
         Device = Device->Next) {
        if (Device->Value.Type != ACPI_DEVICE) {
            continue;
        }

        /* The spec says that we should assume STA is 0x0F (present and all right) if it doesn't
           exist. */
        AcpiObject *StaObject = AcpiSearchObject(Device, "_STA");
        AcpiValue StaValue;
        uint64_t Sta = 0;

        if (StaObject) {
            if (AcpiEvaluateObject(StaObject, &StaValue, ACPI_INTEGER)) {
                Sta = StaValue.Integer;
            }
        } else {
            Sta = 0x0F;
        }

        if (Sta & 1) {
            /* _INI should be a method? Change this to EvaluateObject if we encounter any case of
               this not being one (and it making any difference). */
            AcpiExecuteMethod(AcpiSearchObject(Device, "_INI"), 0, NULL, NULL);

            for (AcpiObject *Region = Device->Value.Children->Objects; Region != NULL;
                 Region = Region->Next) {
                if (Region->Value.Type != ACPI_REGION ||
                    Region->Value.Region.RegionSpace == ACPI_SPACE_SYSTEM_MEMORY ||
                    Region->Value.Region.RegionSpace == ACPI_SPACE_SYSTEM_IO) {
                    continue;
                }

                AcpiValue Arguments[2];
                Arguments[0].References = 1;
                Arguments[0].Integer = Region->Value.Region.RegionSpace;
                Arguments[1].Type = ACPI_INTEGER;
                Arguments[1].References = 1;
                Arguments[1].Integer = 1;

                AcpiExecuteMethod(AcpiSearchObject(Device, "_REG"), 2, Arguments, NULL);
            }
        }

        if ((Sta & 1) | (Sta & 8)) {
            InitializeChildren(Device);
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point of the ACPI compatibility module. We're responsible for
 *     getting things ready so that other drivers can use ACPI functions (such as routing PCI
 *     IRQs).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DriverEntry(void) {
    /* Populate the ACPI namespace. */
    AcpipPopulatePredefined();
    AcpipPopulateOverride();
    AcpipReadTables();

    /* We need _SB_ for the initialization process. AcpipPopulatePredefined should have created it,
       so we're assuming memory corruption if it doesn't exist. */
    AcpiObject *SbScope = AcpiSearchObject(NULL, "_SB_");
    if (!SbScope) {
        AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, "cannot find the \\_SB_ object\n");
    }

    /* Non standard, but call the Windows-specific global initialization method. */
    AcpiExecuteMethod(AcpiSearchObject(NULL, "_INI"), 0, NULL, NULL);

    /* Call the _SB_ scope initialization method. */
    AcpiExecuteMethod(AcpiSearchObject(SbScope, "_INI"), 0, NULL, NULL);

    /* Initialize all devices inside the _SB_ scope. */
    InitializeChildren(SbScope);

    /* Inform the ACPI BIOS that we're still using the classic PIC. */
    AcpiValue Argument;
    Argument.Type = ACPI_INTEGER;
    Argument.References = 1;
    Argument.Integer = 0;
    AcpiExecuteMethod(AcpiSearchObject(NULL, "_PIC"), 1, &Argument, NULL);

    /* For the final step, we need to inform the BIOS we're ready to handle the ACPI/power events
       now.. */
    FadtHeader *Fadt = (FadtHeader *)AcpipFindTable("FACP", 0);
    if (!Fadt) {
        AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, "cannot find the FADT\n");
    }

    if (Fadt->SmiCommandPort && Fadt->AcpiEnable &&
        !(AcpipReadIoSpace(Fadt->Pm1aControlBlock, 2) & 1)) {
        AcpipWriteIoSpace(Fadt->SmiCommandPort, 1, Fadt->AcpiEnable);
        while (!(AcpipReadIoSpace(Fadt->Pm1aControlBlock, 2) & 1))
            ;
    }
    AcpipShowDebugMessage("enabled ACPI\n");

    /* For testing purposes, let's do an S5 shutdown; Start by evaluating \\_S5_. */
    AcpiValue S5;
    if (!AcpiEvaluateObject(AcpiSearchObject(NULL, "_S5_"), &S5, ACPI_PACKAGE)) {
        AcpipShowDebugMessage("machine does not support S5 shutdown\n");
        return;
    }

    /* Validate if we have a package with at least one integer. */
    AcpiValue *SlpTypA = &S5.Package->Data[0].Value;
    AcpiValue *SlpTypB = &S5.Package->Data[1].Value;
    if (S5.Package->Size < 2) {
        AcpipShowDebugMessage("invalid \\_S5_ object, less than 2 package entries\n");
        return;
    } else if (
        (!S5.Package->Data[0].Type || SlpTypA->Type != ACPI_INTEGER) ||
        (!S5.Package->Data[1].Type || SlpTypB->Type != ACPI_INTEGER)) {
        AcpipShowDebugMessage("invalid \\_S5_ object, one of the 2 entries isn't an integer\n");
        return;
    }
    AcpipShowDebugMessage("evaluated \\_S5_\n");

    /* Execute _PTS and _GTS if they exist. */
    Argument.Integer = 5;
    AcpipShowDebugMessage("running \\_PTS\n");
    AcpiExecuteMethod(AcpiSearchObject(NULL, "_PTS"), 1, &Argument, NULL);
    AcpipShowDebugMessage("running \\_GTS\n");
    AcpiExecuteMethod(AcpiSearchObject(NULL, "_GTS"), 1, &Argument, NULL);

    /* Enter S5/shutdown. */
    AcpipShowDebugMessage("running shutdown command\n");
    AcpipWriteIoSpace(Fadt->Pm1aControlBlock, 2, (SlpTypA->Integer << 10) | (1 << 13));
    if (Fadt->Pm1bControlBlock) {
        AcpipWriteIoSpace(Fadt->Pm1bControlBlock, 2, (SlpTypB->Integer << 10) | (1 << 13));
    }

    while (1)
        ;
}
