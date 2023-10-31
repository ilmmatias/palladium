/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <mm.h>
#include <pcip.h>
#include <string.h>

PcipBusList *PcipBusListHead = NULL;

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
    PcipBusList *Parent = PcipBusListHead;
    while (Parent && Parent->Next) {
        Parent = Parent->Next;
    }

    PcipBusList *Entry = MmAllocateBlock(sizeof(PcipBusList));
    if (!Entry) {
        PcipShowErrorMessage(KE_EARLY_MEMORY_FAILURE, "could not allocate space for a PCI bus\n");
    }

    memcpy(&Entry->Bus, Bus, sizeof(PcipBus));
    Entry->Next = NULL;

    if (Parent) {
        Parent->Next = Entry;
    } else {
        PcipBusListHead = Entry;
    }
}
