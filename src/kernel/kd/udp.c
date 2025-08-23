/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>
#include <string.h>

extern void *KdpDebugAdapter;

extern uint8_t KdpDebuggeeHardwareAddress[6];
extern uint8_t KdpDebuggeeProtocolAddress[4];
extern uint16_t KdpDebuggeePort;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends an UDP packet.
 *
 * PARAMETERS:
 *     DestinationHardwareAddress - MAC address of who should receive this packet.
 *     DestinationProtocolAddress - IP(v4) address of who should receive this packet.
 *     SourcePort - UDP port of who is sending this packet.
 *     DestinationPort - UDP port of who should receive this packet.
 *     Buffer - Buffer containing what we should send.
 *     Size - Size of what we should send.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpSendUdpPacket(
    uint8_t DestinationHardwareAddress[6],
    uint8_t DestinationProtocolAddress[4],
    uint16_t SourcePort,
    uint16_t DestinationPort,
    void *Buffer,
    size_t Size) {
    uint32_t Handle = 0;
    uint32_t Status = KdpGetTxPacket(KdpDebugAdapter, &Handle);
    if (Status) {
        return Status;
    }

    KdpEthernetHeader *EthFrame = KdpGetPacketAddress(KdpDebugAdapter, Handle);
    if (!EthFrame) {
        KdpSendTxPacket(KdpDebugAdapter, Handle, 0);
        return 0xC0000004;
    }

    /* Build the ethernet header. */
    memcpy(EthFrame->DestinationAddress, DestinationHardwareAddress, 6);
    memcpy(EthFrame->SourceAddress, KdpDebuggeeHardwareAddress, 6);
    EthFrame->Type = KdpSwapNetworkOrder16(0x0800);

    /* Build the IP header right after it. */
    KdpIpHeader *IpFrame = (void *)(EthFrame + 1);
    IpFrame->Version = 4;
    IpFrame->Length = sizeof(KdpIpHeader) / sizeof(uint32_t);
    IpFrame->TypeOfService = 0;
    IpFrame->TotalLength = KdpSwapNetworkOrder16(sizeof(KdpIpHeader) + sizeof(KdpUdpHeader) + Size);
    IpFrame->Identifier = 0;
    IpFrame->Flags = 0;
    IpFrame->FragmentOffset = 0;
    IpFrame->TimeToLive = 64;
    IpFrame->Protocol = 17;
    IpFrame->HeaderChecksum = 0;
    memcpy(IpFrame->SourceAddress, KdpDebuggeeProtocolAddress, 4);
    memcpy(IpFrame->DestinationAddress, DestinationProtocolAddress, 4);
    IpFrame->HeaderChecksum = KdpSwapNetworkOrder16(KdpCalculateIpChecksum(IpFrame));

    /* And finally build the UDP header at end (and don't forget to copy in the user data!) */
    KdpUdpHeader *UdpFrame = (void *)(IpFrame + 1);
    UdpFrame->SourcePort = KdpSwapNetworkOrder16(SourcePort);
    UdpFrame->DestinationPort = KdpSwapNetworkOrder16(DestinationPort);
    UdpFrame->Length = KdpSwapNetworkOrder16(sizeof(KdpUdpHeader) + Size);
    UdpFrame->Checksum = 0;
    memcpy(UdpFrame + 1, Buffer, Size);

    return KdpSendTxPacket(
        KdpDebugAdapter,
        Handle,
        sizeof(KdpEthernetHeader) + sizeof(KdpIpHeader) + sizeof(KdpUdpHeader) + Size);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received UDP packet.
 *
 * PARAMETERS:
 *     SourceHardwareAddress - MAC address of who sent us this packet.
 *     SourceProtocolAddress - IP(v4) address of who sent us this packet.
 *     UdpFrame - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseUdpFrame(
    uint8_t SourceHardwareAddress[6],
    uint8_t SourceProtocolAddress[4],
    KdpUdpHeader *UdpFrame,
    uint32_t Length) {
    if (Length < sizeof(KdpUdpHeader)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid UDP packet of size %u\n", Length);
        return;
    }

    /* Check for incoming connections on the debug port, and pass them along to the debug packet
     * handler. */
    if (KdpSwapNetworkOrder16(UdpFrame->DestinationPort) != KdpDebuggeePort) {
        return;
    }

    KdpParseDebugPacket(
        SourceHardwareAddress,
        SourceProtocolAddress,
        KdpSwapNetworkOrder16(UdpFrame->SourcePort),
        (void *)(UdpFrame + 1),
        Length - sizeof(KdpUdpHeader));
}
