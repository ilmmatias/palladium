/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <ke.h>
#include <mm.h>
#include <pcip.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the entry point of the PCI bus module. We're responsible for finding all
 *     root buses, and getting things ready for all drivers that depend on us.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void DriverEntry(void) {
    /* Following the ACPI spec, all root buses should be inside the _SB_ namespace. We're searching
       for either legacy PCI buses, or PCIe buses. */
    AcpiObject *SystemBus = AcpiSearchObject(NULL, "_SB_");
    if (!SystemBus) {
        PcipShowErrorMessage(KE_BAD_ACPI_TABLES, "cannot find the \\_SB_ ACPI object\n");
    }

    for (AcpiObject *Device = SystemBus->Value.Children->Objects; Device != NULL;
         Device = Device->Next) {
        AcpiValue Value;
        if ((!AcpiEvaluateObject(AcpiSearchObject(Device, "_HID"), &Value, ACPI_INTEGER) &&
             !AcpiEvaluateObject(AcpiSearchObject(Device, "_CID"), &Value, ACPI_INTEGER)) ||
            (Value.Integer != 0x030AD041 && Value.Integer != 0x080AD041)) {
            continue;
        }

        PcipBus *Bus = MmAllocatePool(sizeof(PcipBus), "Pci ");
        if (!Bus) {
            PcipShowErrorMessage(KE_OUT_OF_MEMORY, "could not allocate space for a PCI bus\n");
        }

        memset(Bus, 0, sizeof(PcipBus));
        Bus->Object = Device;

        if (AcpiEvaluateObject(AcpiSearchObject(Device, "_SEG"), &Value, ACPI_INTEGER)) {
            Bus->Seg = Value.Integer;
        }

        if (AcpiEvaluateObject(AcpiSearchObject(Device, "_BBN"), &Value, ACPI_INTEGER)) {
            Bus->Bbn = Value.Integer;
        }

        PcipInitializeBus(Bus);
        PcipShowInfoMessage("initialized root bus at ACPI path \\_SB_.%.4s\n", Device->Name);
    }
}
