/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
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
    uint8_t *HeaderData = (uint8_t *)Header;
    size_t HeaderLength = (size_t)(HeaderData[0] & 0x0F) * sizeof(uint32_t);
    uint32_t Sum = 0;

    for (size_t i = 0; i < HeaderLength; i += sizeof(uint16_t)) {
        Sum += ((uint16_t)HeaderData[i] << 8) | HeaderData[i + 1];
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

    uint8_t *IpData = (uint8_t *)IpFrame;
    uint8_t Version = IpData[0] >> 4;
    uint8_t HeaderLengthWords = IpData[0] & 0x0F;
    if (Version != 4 || HeaderLengthWords < sizeof(KdpIpHeader) / sizeof(uint32_t)) {
        KdPrint(KD_TYPE_TRACE, "ignoring IP packet with an invalid header\n");
        return;
    }

    uint32_t HeaderLength = (uint32_t)HeaderLengthWords * sizeof(uint32_t);
    if (HeaderLength > Length) {
        KdPrint(KD_TYPE_TRACE, "ignoring truncated IP header\n");
        return;
    }

    uint32_t TotalLength = ((uint32_t)IpData[2] << 8) | IpData[3];
    if (TotalLength < HeaderLength || TotalLength > Length) {
        KdPrint(KD_TYPE_TRACE, "ignoring IP packet with an invalid total length\n");
        return;
    }

    uint16_t FragmentField = ((uint16_t)IpData[6] << 8) | IpData[7];
    if (FragmentField & 0xBFFF) {
        KdPrint(KD_TYPE_TRACE, "ignoring fragmented IP packet\n");
        return;
    }

    /* Reject any packets without a proper checksum (as the IPv4 checksum is required, unlike the
     * UDP checksum). */
    if (KdpCalculateIpChecksum(IpFrame) != 0) {
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
        (void *)(IpData + HeaderLength),
        TotalLength - HeaderLength);
}
