/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpi.h>
#include <acpip.h>
#include <kernel/hal.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads data from the PCI(e) address space.
 *
 * PARAMETERS:
 *     Source - Region containing the data of the PCI device.
 *     Offset - Offset were trying to read.
 *     Size - How many bytes we're trying to read.
 *
 * RETURN VALUE:
 *     Data from the address space.
 *-----------------------------------------------------------------------------------------------*/
uint64_t AcpipReadPciConfigSpace(AcpiValue *Source, int Offset, int Size) {
    AcpipShowTraceMessage(
        "read from PCI config space, %X/%X/%X/%X, offset 0x%X, size %u\n",
        Source->Region.PciSegment,
        Source->Region.PciBus,
        Source->Region.PciDevice,
        Source->Region.PciFunction,
        Offset,
        Size);

    /* TODO: We're kinda missing segment support, as the kernel isn't doing anything to the MCFG
     * table yet... */
    uint64_t Value = 0;
    HalReadPciConfigurationSpace(
        Source->Region.PciBus,
        Source->Region.PciDevice,
        Source->Region.PciFunction,
        Offset,
        &Value,
        Size > 8 ? 8 : Size);
    return Value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the PCI(e) address space.
 *
 * PARAMETERS:
 *     Source - Region containing the data of the PCI device.
 *     Offset - Offset were trying to read.
 *     Size - How many bytes we're trying to write.
 *     Data - What we're trying to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipWritePciConfigSpace(AcpiValue *Source, int Offset, int Size, uint64_t Data) {
    AcpipShowTraceMessage(
        "write into PCI config space, %X/%X/%X/%X, offset 0x%X, size %u, data 0x%llX\n",
        Source->Region.PciSegment,
        Source->Region.PciBus,
        Source->Region.PciDevice,
        Source->Region.PciFunction,
        Offset,
        Size,
        Data);

    HalWritePciConfigurationSpace(
        Source->Region.PciBus,
        Source->Region.PciDevice,
        Source->Region.PciFunction,
        Offset,
        &Data,
        Size > 8 ? 8 : Size);
}
