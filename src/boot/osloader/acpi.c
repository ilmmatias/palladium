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
 *     AcpiRootPointer - Output; Where to store the RSDP.
 *     AcpiRootTable - Output; Where to store the RSDT/XSDT.
 *     AcpiRootTableSize - Output; Size in bytes of the RSDT/XSDT.
 *     AcpiVersion - Output; Where to store the version of the ACPI root table.
 *
 * RETURN VALUE:
 *     true on success, false if we didn't find the table.
 *-----------------------------------------------------------------------------------------------*/
bool OslpInitializeAcpi(
    void **AcpiRootPointer,
    void **AcpiRootTable,
    uint32_t *AcpiRootTableSize,
    uint32_t *AcpiVersion) {
    bool Status = false;

    for (UINTN i = 0; i < gST->NumberOfTableEntries; i++) {
        RsdpHeader *Rsdp = gST->ConfigurationTable[i].VendorTable;

        if (!memcmp(
                &gST->ConfigurationTable[i].VendorGuid, &gEfiAcpi20TableGuid, sizeof(EFI_GUID))) {
            *AcpiRootPointer = (void *)(UINTN)Rsdp;
            *AcpiRootTable = (void *)(UINTN)Rsdp->XsdtAddress;
            *AcpiVersion = 2;
            Status = true;
            /* ACPI 2.0 is our target, we can stop if we find it. */
            break;
        }

        if (!memcmp(
                &gST->ConfigurationTable[i].VendorGuid, &gEfiAcpi10TableGuid, sizeof(EFI_GUID))) {
            *AcpiRootPointer = (void *)(UINTN)Rsdp;
            *AcpiRootTable = (void *)(UINTN)Rsdp->RsdtAddress;
            *AcpiVersion = 1;
            Status = true;
        }
    }

    if (!Status) {
        OslPrint("Failed to obtain the ACPI root table pointer.\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
    }

    *AcpiRootTableSize = ((SdtHeader *)*AcpiRootTable)->Length;
    return Status;
}
