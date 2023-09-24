/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <amd64/port.h>

uint64_t AcpipReadPciConfigSpace(AcpiValue *Source, int Offset, int Size) {
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

void AcpipWritePciConfigSpace(AcpiValue *Source, int Offset, int Size, uint64_t Data) {
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
