/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/apic.h>
#include <amd64/halp.h>

extern RtSList HalpLapicListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function notifies all online processors that an important kernel event has happened.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiNotifyProcessors(void) {
    RtSList *ListHeader = HalpLapicListHead.Next;
    while (ListHeader) {
        LapicEntry *Entry = CONTAINING_RECORD(ListHeader, LapicEntry, ListHeader);

        if (Entry->Online) {
            HalpSendIpi(Entry->ApicId, 0xFE);
            HalpWaitIpiDelivery();
        }

        ListHeader = ListHeader->Next;
    }
}
