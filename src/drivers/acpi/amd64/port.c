/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/port.h>
#include <stdio.h>

uint64_t AcpipReadIoSpace(int Offset, int Size) {
    switch (Size) {
        case 1:
            return ReadPortByte(Offset);
        case 2:
            return ReadPortWord(Offset);
        default:
            return ReadPortDWord(Offset);
    }
}

void AcpipWriteIoSpace(int Offset, int Size, uint64_t Data) {
    switch (Size) {
        case 1:
            return WritePortByte(Offset, Data);
        case 2:
            return WritePortWord(Offset, Data);
        default:
            return WritePortDWord(Offset, Data);
    }
}