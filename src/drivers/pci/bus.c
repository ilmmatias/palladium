/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <pcip.h>

RtSList PcipBusListHead = {};

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
    RtPushSList(&PcipBusListHead, &Bus->ListHeader);
}
