/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>
#include <string.h>

extern uint8_t KdpDebuggeeProtocolAddress[4];

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates the checksum for an IP(v4) header.
 *
 * PARAMETERS:
 *     Header - What we should calculate the checksum of.
 *
 * RETURN VALUE:
 *     Checksum value, in the host byte ordering.
 *-----------------------------------------------------------------------------------------------*/
uint16_t KdpCalculateIpChecksum(KdpIpHeader *Header) {
    uint16_t *HeaderData = (uint16_t *)Header;
    uint32_t Sum = 0;

    for (size_t i = 0; i < Header->Length * 2; i++) {
        Sum += KdpSwapNetworkOrder16(HeaderData[i]);
    }

    while (Sum >> 16) {
        Sum = (Sum & 0xFFFF) + (Sum >> 16);
    }

    return ~Sum;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received IP(v4) packet.
 *
 * PARAMETERS:
 *     State - Current execution state (initialization or break/panic).
 *     SourceHardwareAddress - MAC address of who sent us this packet.
 *     IpFrame - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseIpFrame(
    int State,
    uint8_t SourceHardwareAddress[6],
    KdpIpHeader *IpFrame,
    uint32_t Length) {
    if (Length < sizeof(KdpIpHeader)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid IP packet of size %u\n", Length);
        return;
    } else if (IpFrame->Version != 4) {
        return;
    }

    /* Reject any packets without a proper checksum (as the IPv4 checksum is required, unlike the
     * UDP checksum). */
    uint16_t HeaderChecksum = KdpSwapNetworkOrder16(IpFrame->HeaderChecksum);
    IpFrame->HeaderChecksum = 0;
    if (HeaderChecksum != KdpCalculateIpChecksum(IpFrame)) {
        KdPrint(KD_TYPE_TRACE, "ignoring IP packet without a valid checksum\n");
        return;
    }

    /* We also only care about UDP, so ignore anything else (while we're at it, also validate that
     * the target IP address is correct/ours). */
    if (memcmp(IpFrame->DestinationAddress, KdpDebuggeeProtocolAddress, 4) ||
        IpFrame->Protocol != 17) {
        return;
    }

    /* If all is well, pass forward to the UDP handler. */
    KdpParseUdpFrame(
        State,
        SourceHardwareAddress,
        IpFrame->SourceAddress,
        (void *)(IpFrame + 1),
        Length - sizeof(KdpUdpHeader));
}
