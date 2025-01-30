/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <console.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries searching for and saving the pointer into the root ACPI table.
 *
 * PARAMETERS:
 *     AcpiTable - Output; Where to store the pointer.
 *     AcpiTableVersion - Output; Where to store the version of the ACPI root table.
 *
 * RETURN VALUE:
 *     true on success, false if we didn't find the table.
 *-----------------------------------------------------------------------------------------------*/
bool OslpInitializeAcpi(void **AcpiTable, uint32_t *AcpiTableVersion) {
    bool Status = false;

    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        RsdpHeader *Rsdp = gST->ConfigurationTable[i].VendorTable;

        if (!memcmp(
                &gST->ConfigurationTable[i].VendorGuid, &gEfiAcpi20TableGuid, sizeof(EFI_GUID))) {
            *AcpiTable = (void *)(UINTN)Rsdp->XsdtAddress;
            *AcpiTableVersion = 2;
            Status = true;
            /* ACPI 2.0 is our target, we can stop if we find it. */
            break;
        }

        if (!memcmp(
                &gST->ConfigurationTable[i].VendorGuid, &gEfiAcpi10TableGuid, sizeof(EFI_GUID))) {
            *AcpiTable = (void *)(UINTN)Rsdp->RsdtAddress;
            *AcpiTableVersion = 1;
            Status = true;
        }
    }

    if (!Status) {
        OslPrint("Failed to obtain the ACPI root table pointer.\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
    }

    return Status;
}
