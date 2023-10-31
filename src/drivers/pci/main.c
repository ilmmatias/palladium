/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpi.h>
#include <ke.h>
#include <pcip.h>

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
        PcipShowErrorMessage(
            KE_CORRUPTED_HARDWARE_STRUCTURES, "cannot find the \\_SB_ ACPI object\n");
    }

    for (AcpiObject *Bus = SystemBus->Value.Children->Objects; Bus != NULL; Bus = Bus->Next) {
        AcpiValue Id;
        if ((!AcpiEvaluateObject(AcpiSearchObject(Bus, "_HID"), &Id, ACPI_INTEGER) &&
             !AcpiEvaluateObject(AcpiSearchObject(Bus, "_CID"), &Id, ACPI_INTEGER)) ||
            (Id.Integer != 0x030AD041 && Id.Integer != 0x080AD041)) {
            continue;
        }

        AcpiValue Seg;
        uint64_t SegValue = 0;
        if (AcpiEvaluateObject(AcpiSearchObject(Bus, "_SEG"), &Seg, ACPI_INTEGER)) {
            SegValue = Seg.Integer;
        }

        AcpiValue Bbn;
        uint64_t BbnValue = 0;
        if (AcpiEvaluateObject(AcpiSearchObject(Bus, "_BBN"), &Bbn, ACPI_INTEGER)) {
            BbnValue = Bbn.Integer;
        }

        PcipShowInfoMessage("initialized root bus at ACPI path \\_SB_.%.4s\n", Bus->Name);

        (void)SegValue;
        (void)BbnValue;
    }
}
