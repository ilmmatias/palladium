/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>
#include <stdint.h>

static uint64_t AcpiBaseAddress;
static int AcpiTableType;

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
    AcpiBaseAddress = BootData->Acpi.BaseAdress;
    AcpiTableType = BootData->Acpi.IsXsdt + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function obtains the base address of the RSDT/XSDT, as saved by KiSaveAcpiData.
 *     Do not use this if you don't know what you're doing! The main purpose of this is exposing
 *     the required info for acpi.sys.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Base address of the RSDT/XSDT (physical!).
 *-----------------------------------------------------------------------------------------------*/
uint64_t KiGetAcpiBaseAddress(void) {
    return AcpiBaseAddress;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function obtains the type of the table pointed by KiGetAcpiBaseAddress.
 *     Do not use this if you don't know what you're doing! The main purpose of this is exposing
 *     the required info for acpi.sys.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Either KI_ACPI_RSDT or KI_ACPI_XSDT, according to the type of the table.
 *-----------------------------------------------------------------------------------------------*/
int KiGetAcpiTableType(void) {
    return AcpiTableType;
}
