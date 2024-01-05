/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <amd64/port.h>

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
uint64_t AcpipReadIoSpace(int Offset, int Size) {
    AcpipShowTraceMessage("read from IO space, port 0x%hX, size %u\n", (uint16_t)Offset, Size);

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
void AcpipWriteIoSpace(int Offset, int Size, uint64_t Data) {
    AcpipShowTraceMessage(
        "write into IO space, port 0x%hX, size %u, data 0x%llX\n", (uint16_t)Offset, Size, Data);

    switch (Size) {
        case 1:
            return WritePortByte(Offset, Data);
        case 2:
            return WritePortWord(Offset, Data);
        default:
            return WritePortDWord(Offset, Data);
    }
}