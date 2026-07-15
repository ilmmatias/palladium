/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <os/intrin.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads data from a specific port.
 *
 * PARAMETERS:
 *     Offset - Port number; The high 16-bits will be ignored.
 *     Size - How many bytes we're trying to read.
 *
 * RETURN VALUE:
 *     Data from the address space.
 *-----------------------------------------------------------------------------------------------*/
uint64_t AcpipReadIoSpace(uint64_t Offset, uint64_t Size) {
    AcpipShowTraceMessage("read from IO space, port 0x%llX, size %llu\n", Offset, Size);

    switch (Size) {
        case 1:
            return ReadPortByte(Offset);
        case 2:
            return ReadPortWord(Offset);
        default:
            return ReadPortDWord(Offset);
    }
}
/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes data into a specific port.
 *
 * PARAMETERS:
 *     Offset - Port number; The high 16-bits will be ignored.
 *     Size - How many bytes we're trying to write.
 *     Data - What we're trying to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipWriteIoSpace(uint64_t Offset, uint64_t Size, uint64_t Data) {
    AcpipShowTraceMessage(
        "write into IO space, port 0x%llX, size %llu, data 0x%llX\n", Offset, Size, Data);

    switch (Size) {
        case 1:
            return WritePortByte(Offset, Data);
        case 2:
            return WritePortWord(Offset, Data);
        default:
            return WritePortDWord(Offset, Data);
    }
}
