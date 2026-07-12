/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <stdint.h>

/* TODO: Should this data be in a header? */
#define ADDRESS_PORT 0x70
#define DATA_PORT 0x71
#define NMI_MASK 0x80
#define OFFSET_MASK 0x7F

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads a byte from the CMOS/RTC space.
 *
 * PARAMETERS:
 *     Offset - CMOS address to read from.
 *
 * RETURN VALUE:
 *     The byte read from CMOS.
 *-----------------------------------------------------------------------------------------------*/
uint64_t AcpipReadCmosSpace(uint64_t Offset) {
    uint8_t CmosData = AcpipReadIoSpace(ADDRESS_PORT, 1);
    AcpipWriteIoSpace(ADDRESS_PORT, 1, (CmosData & NMI_MASK) | (Offset & OFFSET_MASK));
    return AcpipReadIoSpace(DATA_PORT, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes a byte to the CMOS/RTC space.
 *
 * PARAMETERS:
 *     Offset - CMOS address to write to.
 *     Data - The byte to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipWriteCmosSpace(uint64_t Offset, uint64_t Data) {
    uint8_t CmosData = AcpipReadIoSpace(ADDRESS_PORT, 1);
    AcpipWriteIoSpace(ADDRESS_PORT, 1, (CmosData & NMI_MASK) | (Offset & OFFSET_MASK));
    AcpipWriteIoSpace(DATA_PORT, 1, Data & UINT8_MAX);
}
