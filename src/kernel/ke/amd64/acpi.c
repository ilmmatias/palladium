/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>

extern uint64_t KiAcpiBaseAddress;
extern int KiAcpiTableType;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the ACPI main (RSDT/XSDT) table base address, which the ACPI driver
 *     can access through the KiAcpi* functions.
 *
 * PARAMETERS:
 *     LoaderData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSaveAcpiData(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;
    KiAcpiBaseAddress = BootData->Acpi.BaseAdress;
    KiAcpiTableType = BootData->Acpi.IsXsdt + 1;
}
