/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.hxx>
#include <os.hxx>

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
extern "C" void DriverEntry(void) {
    SdtHeader *Header = AcpipFindTable("DSDT", 0);
    if (!Header) {
        AcpipShowErrorMessage(KE_BAD_ACPI_TABLES, "couldn't find the DSDT table\n");
    }

    AcpipInitializeBuiltin();
    AcpipShowInfoMessage("enabled ACPI\n");
}
