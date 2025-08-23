/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>
#include <string.h>

extern void *KdpDebugAdapter;

extern uint8_t KdpDebuggeeHardwareAddress[6];
extern uint8_t KdpDebuggeeProtocolAddress[4];
extern uint16_t KdpDebuggeePort;

extern uint8_t KdpDebuggerHardwareAddress[6];
extern uint8_t KdpDebuggerProtocolAddress[4];
extern uint16_t KdpDebuggerPort;
extern bool KdpDebuggerConnected;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received debug packet.
 *
 * PARAMETERS:
 *     SourceHardwareAddress - MAC address of who sent us this packet.
 *     SourceProtocolAddress - IP(v4) address of who sent us this packet.
 *     SourcePort - UDP port of who sent us this packet.
 *     Packet - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseDebugPacket(
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    uint16_t SourcePort,
    KdpDebugPacket *Packet,
    uint32_t Length) {
    if (Length < sizeof(KdpDebugPacket)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug packet of size %u\n", Length);
        return;
    }

    /* We only support one incoming message for now (the connection request), so parse it and reply
     * with an ACK. */
    if (Packet->Type != KDP_DEBUG_PACKET_CONNECT_REQ) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid debug packet of type %hhu\n", Packet->Type);
        return;
    }

    KdpDebugPacket AckPacket;
    AckPacket.Type = KDP_DEBUG_PACKET_CONNECT_ACK;

    uint32_t Status = KdpSendUdpPacket(
        SourceHardwareAddress,
        SourceProtocolAddress,
        KdpDebuggeePort,
        SourcePort,
        &AckPacket,
        sizeof(KdpDebugPacket));
    if (Status) {
        KdPrint(
            KD_TYPE_ERROR,
            "failed to ack the debugger connection request, error code = 0x%08x\n",
            Status);
    } else {
        KdpDebuggerConnected = true;
        memcpy(KdpDebuggerHardwareAddress, SourceHardwareAddress, 6);
        memcpy(KdpDebuggerProtocolAddress, SourceProtocolAddress, 4);
        KdpDebuggerPort = SourcePort;
        KdPrint(
            KD_TYPE_TRACE,
            "sent debugger connection ack to %hhu.%hhu.%hhu.%hhu:%hu\n",
            SourceProtocolAddress[0],
            SourceProtocolAddress[1],
            SourceProtocolAddress[2],
            SourceProtocolAddress[3],
            SourcePort);
    }
}
