/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <pcip.h>

RtSinglyLinkedListEntry PcipBusListHead = {.Next = NULL};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes and registers a PCI bus or bridge, for later enumeration. This
 *     function does not handle enumerating the bus for children bridges, that's
 *     PcipEnumerateBridges() job.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PcipInitializeBus(PcipBus *Bus) {
    RtPushSinglyLinkedList(&PcipBusListHead, &Bus->ListHeader);
}
