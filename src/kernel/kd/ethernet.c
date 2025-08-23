/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a received ethernet packet.
 *
 * PARAMETERS:
 *     EthFrame - Header of the packet.
 *     Length - Size of the packet.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpParseEthernetFrame(KdpEthernetHeader *EthFrame, uint32_t Length) {
    if (Length < sizeof(KdpEthernetHeader)) {
        KdPrint(KD_TYPE_TRACE, "ignoring invalid ethernet packet of size %u\n", Length);
        return;
    }

    /* Pass along the header depending on the type (just remember to handle network word ordering
     * more than likely not being equal to host word ordering). */
    switch (KdpSwapNetworkOrder16(EthFrame->Type)) {
        case 0x0800:
            KdpParseIpFrame(
                EthFrame->SourceAddress,
                (void *)(EthFrame + 1),
                Length - sizeof(KdpEthernetHeader));
            break;
        case 0x0806:
            KdpParseArpFrame((void *)(EthFrame + 1), Length - sizeof(KdpEthernetHeader));
            break;
    }
}
