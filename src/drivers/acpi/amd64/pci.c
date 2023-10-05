/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <amd64/port.h>

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

    /* Setup the address we need to send into the CONFIG_ADDRESS port. */
    uint32_t Address = 0x80000000 | (Source->Region.PciBus << 16) |
                       (Source->Region.PciDevice << 11) | (Source->Region.PciFunction << 8) |
                       (Offset & 0xFC);

    /* Send it, and try to read from CONFIG_DATA. */
    WritePortDWord(0xCF8, Address);

    switch (Size) {
        case 1:
            return ReadPortByte(0xCFC + (Offset & 2));
        case 2:
            return ReadPortWord(0xCFC + (Offset & 2));
        default:
            return ReadPortDWord(0xCFC + (Offset & 2));
    }
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

    /* Setup the address we need to send into the CONFIG_ADDRESS port. */
    uint32_t Address = 0x80000000 | (Source->Region.PciBus << 16) |
                       (Source->Region.PciDevice << 11) | (Source->Region.PciFunction << 8) |
                       (Offset & 0xFC);

    /* Send it, followed by writing into CONFIG_DATA. */
    WritePortDWord(0xCF8, Address);

    switch (Size) {
        case 1:
            WritePortByte(0xCFC + (Offset & 2), Data);
            break;
        case 2:
            WritePortWord(0xCFC + (Offset & 2), Data);
            break;
        default:
            WritePortDWord(0xCFC + (Offset & 2), Data);
            break;
    }
}
