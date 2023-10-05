/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <acpip.h>
#include <mm.h>

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

    Address = (uint64_t)MI_PADDR_TO_VADDR(Address);
    switch (Size) {
        case 1:
            return *(uint8_t*)Address;
        case 2:
            return *(uint16_t*)Address;
        case 3:
            return *(uint32_t*)Address;
        default:
            return *(uint64_t*)Address;
    }
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

    Address = (uint64_t)MI_PADDR_TO_VADDR(Address);
    switch (Size) {
        case 1:
            *(uint8_t*)Address = Data;
            break;
        case 2:
            *(uint16_t*)Address = Data;
            break;
        case 3:
            *(uint32_t*)Address = Data;
            break;
        default:
            *(uint64_t*)Address = Data;
            break;
    }
}
