/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <stdarg.h>
#include <stdlib.h>
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

            char *Path = AcpiGetObjectPath(Device);
            if (Path) {
                AcpipShowTraceMessage("initialized device, full path %s\n", Path);
                free(Path);
            } else {
                AcpipShowTraceMessage("initialized device, top most name %.4s\n", Device->Name);
            }

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

    AcpipShowInfoMessage("enabled ACPI\n");
}
