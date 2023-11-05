/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>

extern uint64_t HalpAcpiBaseAddress;
extern int HalpAcpiTableType;

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
void HalpSaveAcpiData(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;
    HalpAcpiBaseAddress = BootData->Acpi.BaseAdress;
    HalpAcpiTableType = BootData->Acpi.IsXsdt + 1;
}
