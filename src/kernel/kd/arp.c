/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>
#include <string.h>

extern void *KdpDebugAdapter;

extern uint8_t KdpDebuggeeHardwareAddress[6];
extern uint8_t KdpDebuggeeProtocolAddress[4];

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sends an ARP packet.
 *
 * PARAMETERS:
 *     Type - Which kind of packet to send.
 *     DestinationEthernetAddress - Destination field for the ethernet header.
 *     DestinationHardwareAddress - Destination field for the ARP header.
 *     DestinationProtocolAddress - Destination IP address.
 *
 * RETURN VALUE:
 *     NTSTATUS values describing the result of the operation (anything but STATUS_SUCCESS is to
 *     be considered an error).
 *-----------------------------------------------------------------------------------------------*/
uint32_t KdpSendArpPacket(
    uint16_t Type,
    uint8_t DestinationEthernetAddress[6],
    uint8_t DestinationHardwareAddress[6],
    uint8_t DestinationProtocolAddress[4]) {
    uint32_t Handle = 0;
    uint32_t Status = KdpGetTxPacket(KdpDebugAdapter, &Handle);
    if (Status) {
        return Status;
    }

    /* Can this even be NULL? We do check just to make sure, but we probably don't need to. */
    KdpEthernetHeader *EthFrame = KdpGetPacketAddress(KdpDebugAdapter, Handle);
    if (!EthFrame) {
        KdpSendTxPacket(KdpDebugAdapter, Handle, 0);
        return 0xC0000004;
    }

    /* Build the ethernet header. */
    memcpy(EthFrame->DestinationAddress, DestinationEthernetAddress, 6);
    memcpy(EthFrame->SourceAddress, KdpDebuggeeHardwareAddress, 6);
    EthFrame->Type = KdpSwapNetworkOrder16(0x0806);

    /* Build the ARP header right after it. */
    KdpArpHeader *ArpFrame = (void *)(EthFrame + 1);
    ArpFrame->HardwareType = KdpSwapNetworkOrder16(1);
    ArpFrame->ProtocolType = KdpSwapNetworkOrder16(0x0800);
    ArpFrame->HardwareAddressLength = 6;
    ArpFrame->ProtocolAddressLength = 4;
    ArpFrame->Operation = KdpSwapNetworkOrder16(Type);
    memcpy(ArpFrame->SourceHardwareAddress, KdpDebuggeeHardwareAddress, 6);
    memcpy(ArpFrame->SourceProtocolAddress, KdpDebuggeeProtocolAddress, 4);
    memcpy(ArpFrame->DestinationHardwareAddress, DestinationHardwareAddress, 6);
    memcpy(ArpFrame->DestinationProtocolAddress, DestinationProtocolAddress, 4);

    return KdpSendTxPacket(
        KdpDebugAdapter, Handle, sizeof(KdpEthernetHeader) + sizeof(KdpArpHeader));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received ARP packet.
 *
 * PARAMETERS:
 *     ArpFrame - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseArpFrame(KdpArpHeader *ArpFrame, uint32_t Length) {
    if (Length < sizeof(KdpArpHeader)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid ARP packet of size %u\n", Length);
        return;
    }

    /* We don't maintain any ARP tables ourselves, so we only care about ARP requests asking for our
     * IP. */
    if (KdpSwapNetworkOrder16(ArpFrame->Operation) != 1 ||
        memcmp(ArpFrame->DestinationProtocolAddress, KdpDebuggeeProtocolAddress, 4)) {
        return;
    }

    /* Now, we do try to reply, but we don't panic if we fail to do so (just report the error). */
    uint32_t Status = KdpSendArpPacket(
        2,
        ArpFrame->SourceHardwareAddress,
        ArpFrame->SourceHardwareAddress,
        ArpFrame->SourceProtocolAddress);
    if (Status) {
        KdPrint(
            KD_TYPE_ERROR,
            "failed to send reply to ARP request packet, error code = 0x%08x\n",
            Status);
    } else {
        KdPrint(
            KD_TYPE_TRACE,
            "sent ARP reply to %hhu.%hhu.%hhu.%hhu\n",
            ArpFrame->SourceProtocolAddress[0],
            ArpFrame->SourceProtocolAddress[1],
            ArpFrame->SourceProtocolAddress[2],
            ArpFrame->SourceProtocolAddress[3]);
    }
}
