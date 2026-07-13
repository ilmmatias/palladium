/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kd.h>
#include <kernel/kdp.h>
#include <stddef.h>
#include <stdint.h>
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

    for (size_t i = 0; i < (size_t)(Header->Length * 2); i++) {
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
    }

    if (IpFrame->Version != 4) {
        return;
    }

    uint32_t HeaderSize = (uint32_t)IpFrame->Length * sizeof(uint32_t);
    uint32_t TotalLength = KdpSwapNetworkOrder16(IpFrame->TotalLength);
    uint16_t FragmentData = 0;
    memcpy(&FragmentData, (char *)IpFrame + 6, sizeof(FragmentData));
    FragmentData = KdpSwapNetworkOrder16(FragmentData);
    if (IpFrame->Length < 5 || HeaderSize > Length || TotalLength < HeaderSize ||
        TotalLength > Length || TotalLength - HeaderSize < sizeof(KdpUdpHeader) ||
        (FragmentData & 0x3FFF)) {
        return;
    }

    /* Reject any packets without a proper checksum (as the IPv4 checksum is required, unlike the
     * UDP checksum). */
    uint16_t HeaderChecksum = KdpSwapNetworkOrder16(IpFrame->HeaderChecksum);
    uint8_t HeaderCopy[60];
    memcpy(HeaderCopy, IpFrame, HeaderSize);
    ((KdpIpHeader *)HeaderCopy)->HeaderChecksum = 0;
    if (HeaderChecksum != KdpCalculateIpChecksum((KdpIpHeader *)HeaderCopy)) {
        KdPrint(KD_TYPE_TRACE, "ignoring IP packet without a valid checksum\n");
        return;
    }

    /* We also only care about UDP, so ignore anything else (while we're at it, also validate that
     * the target IP address is correct/ours). */
    if (memcmp(IpFrame->DestinationAddress, KdpDebuggeeProtocolAddress, 4) != 0 ||
        IpFrame->Protocol != 17) {
        return;
    }

    /* If all is well, pass forward to the UDP handler. */
    KdpParseUdpFrame(
        State,
        SourceHardwareAddress,
        IpFrame->SourceAddress,
        (void *)((char *)IpFrame + HeaderSize),
        TotalLength - HeaderSize);
}
