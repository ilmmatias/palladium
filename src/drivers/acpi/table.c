/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the ACPI subsystem by reading the DSDT + all SSDTs in the system.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipReadTables(void) {
    /* The DSDT should always exist; We assume something's broken with this BIOS if it doesn't. */
    SdtHeader *Dsdt = AcpipFindTable("DSDT", 0);
    if (!Dsdt) {
        AcpipShowErrorMessage(ACPI_REASON_CORRUPTED_TABLES, "couldn't find the DSDT table\n");
    }

    AcpipShowDebugMessage("reading DSDT (%u bytes)\n", Dsdt->Length - sizeof(SdtHeader));
    AcpipPopulateTree((uint8_t *)(Dsdt + 1), Dsdt->Length - sizeof(SdtHeader));

    for (int i = 0;; i++) {
        SdtHeader *Ssdt = AcpipFindTable("SSDT", i);
        if (!Ssdt) {
            break;
        }

        AcpipShowDebugMessage("reading SSDT (%u bytes)\n", Ssdt->Length - sizeof(SdtHeader));
        AcpipPopulateTree((uint8_t *)(Ssdt + 1), Ssdt->Length - sizeof(SdtHeader));
    }
}
