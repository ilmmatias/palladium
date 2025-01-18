/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a clock event (triggers a dispatch event if neessary).
 *
 * PARAMETERS:
 *     Context - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpHandleClock(HalRegisterState *) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    uint64_t CurrentTicks = HalGetTimerTicks();
    int TriggerEvent = 0;

    /* Check our pending events. */
    RtDList *ListHeader = Processor->EventQueue.Next;
    while (ListHeader != &Processor->EventQueue) {
        EvHeader *Header = CONTAINING_RECORD(ListHeader, EvHeader, ListHeader);
        ListHeader = ListHeader->Next;
        if ((Header->Deadline && CurrentTicks >= Header->Deadline) || Header->Finished) {
            TriggerEvent = 1;
        }
    }

    /* Check if we have any DPCs (we should probably only trigger the dispatch event if we have
     * enough DPCs, or one of the other events).*/
    if (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        TriggerEvent = 1;
    }

    /* At last, check for anything that would swap out the thread (we probably only really need to
     * heck quanta expiration though). */
    if (Processor->InitialThread &&
        (Processor->CurrentThread->Terminated ||
         CurrentTicks >= Processor->CurrentThread->Expiration || Processor->ForceYield)) {
        TriggerEvent = 1;
    }

    /* Finally, trigger an event at dispatch IRQL if we need to do anything else. */
    if (TriggerEvent) {
        HalpNotifyProcessor(Processor, 0);
    }
}
