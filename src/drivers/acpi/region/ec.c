/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>

#define CMD_READ 0x80
#define CMD_WRITE 0x81

#define STATUS_OBF 0x01
#define STATUS_IBF 0x02

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function waits for the EC input buffer to be empty.
 *
 * PARAMETERS:
 *     CmdPort - The EC command/status port address.
 *
 * RETURN VALUE:
 *     1 on success, 0 on timeout.
 *-----------------------------------------------------------------------------------------------*/
static int WaitIbfEmpty(uint16_t CmdPort) {
    for (int i = 0; i < 10000; i++) {
        if (!(AcpipReadIoSpace(CmdPort, 1) & STATUS_IBF)) {
            return 1;
        }

        AcpipStallExecution(10);
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function waits for the EC output buffer to be full.
 *
 * PARAMETERS:
 *     CmdPort - The EC command/status port address.
 *
 * RETURN VALUE:
 *     1 on success, 0 on timeout.
 *-----------------------------------------------------------------------------------------------*/
static int WaitObfFull(uint16_t CmdPort) {
    for (int i = 0; i < 10000; i++) {
        if (AcpipReadIoSpace(CmdPort, 1) & STATUS_OBF) {
            return 1;
        }

        AcpipStallExecution(10);
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads a byte from the Embedded Controller space.
 *
 * PARAMETERS:
 *     CmdPort - The EC command/status port address.
 *     DataPort - The EC data port address.
 *     Offset - EC address to read from.
 *
 * RETURN VALUE:
 *     The byte read from the EC.
 *-----------------------------------------------------------------------------------------------*/
uint64_t AcpipReadEcSpace(uint16_t CmdPort, uint16_t DataPort, uint64_t Offset) {
    if (!WaitIbfEmpty(CmdPort)) {
        return 0;
    }

    AcpipWriteIoSpace(CmdPort, 1, CMD_READ);
    if (!WaitIbfEmpty(CmdPort)) {
        return 0;
    }

    AcpipWriteIoSpace(DataPort, 1, Offset & 0xFF);
    if (!WaitObfFull(CmdPort)) {
        return 0;
    }

    return AcpipReadIoSpace(DataPort, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes a byte to the Embedded Controller space.
 *
 * PARAMETERS:
 *     CmdPort - The EC command/status port address.
 *     DataPort - The EC data port address.
 *     Offset - EC address to write to.
 *     Data - The byte to write.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipWriteEcSpace(uint16_t CmdPort, uint16_t DataPort, uint64_t Offset, uint64_t Data) {
    if (!WaitIbfEmpty(CmdPort)) {
        return;
    }
    AcpipWriteIoSpace(CmdPort, 1, CMD_WRITE);
    if (!WaitIbfEmpty(CmdPort)) {
        return;
    }

    AcpipWriteIoSpace(DataPort, 1, Offset & 0xFF);
    if (!WaitIbfEmpty(CmdPort)) {
        return;
    }

    AcpipWriteIoSpace(DataPort, 1, Data & 0xFF);
}
