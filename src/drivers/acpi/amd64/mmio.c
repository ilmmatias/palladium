/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <kernel/mm.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads data from the physical memory.
 *
 * PARAMETERS:
 *     Address - Physical address to be read; We'll convert/map it into a virtual address.
 *     Size - How many bytes we're trying to read.
 *
 * RETURN VALUE:
 *     Data from the address space.
 *-----------------------------------------------------------------------------------------------*/
uint64_t AcpipReadMmioSpace(uint64_t Address, int Size) {
    AcpipShowTraceMessage("read from MMIO space, address 0x%llX, size %u\n", Address, Size);

    void *VirtualAddress = MmMapSpace(MM_SPACE_NORMAL, Address, Size);
    uint64_t Value = 0;

    switch (Size) {
        case 1:
            Value = *(uint8_t *)VirtualAddress;
            break;
        case 2:
            Value = *(uint16_t *)VirtualAddress;
            break;
        case 3:
            Value = *(uint32_t *)VirtualAddress;
            break;
        default:
            Value = *(uint64_t *)VirtualAddress;
            break;
    }

    MmUnmapSpace(VirtualAddress, Size);
    return Value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into the physical memory.
 *
 * PARAMETERS:
 *     Address - Physical address to be written into; We'll convert/map it into a virtual address.
 *     Size - How many bytes we're trying to write.
 *     Data - What we're trying to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipWriteMmioSpace(uint64_t Address, int Size, uint64_t Data) {
    AcpipShowTraceMessage(
        "write into MMIO space, address 0x%llX, size %u, data 0x%llX\n", Address, Size, Data);

    void *VirtualAddress = MmMapSpace(MM_SPACE_NORMAL, Address, Size);

    switch (Size) {
        case 1:
            *(uint8_t *)VirtualAddress = Data;
            break;
        case 2:
            *(uint16_t *)VirtualAddress = Data;
            break;
        case 3:
            *(uint32_t *)VirtualAddress = Data;
            break;
        default:
            *(uint64_t *)VirtualAddress = Data;
            break;
    }

    MmUnmapSpace(VirtualAddress, Size);
}
