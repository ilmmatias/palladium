/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <evp.h>
#include <halp.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given timer event, setting its deadline relative to the
 *     current time.
 *
 * PARAMETERS:
 *     Timer - Pointer to the timer struct.
 *     Timeout - How many nanoseconds to sleep for.
 *     Dpc - Optional DPC to be executed/enqueued when we finish.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvInitializeTimer(EvTimer *Timer, uint64_t Timeout, EvDpc *Dpc) {
    memset(Timer, 0, sizeof(EvTimer));
    Timer->Type = EV_TYPE_TIMER;
    Timer->Dpc = Dpc;

    /* Dispatch the DPC if this is 0-timeout event, and don't bother with the event dispatcher
       (just set us as finished). */
    if (!Timeout) {
        Timer->Finished = 1;
        EvDispatchDpc(Dpc);
        return;
    }

    EvpDispatchObject(Timer, Timeout, 0);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a clock event (triggers a dispatch event if neessary).
 *
 * PARAMETERS:
 *     None that we make use of.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpHandleTimer(HalInterruptFrame *) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    uint64_t CurrentTicks = HalGetTimerTicks();
    int TriggerEvent = 0;

    /* Check our pending events. */
    RtDList *ListHeader = Processor->EventQueue.Next;
    while (ListHeader != &Processor->EventQueue) {
        EvHeader *Header = CONTAINING_RECORD(ListHeader, EvHeader, ListHeader);
        ListHeader = ListHeader->Next;
        if ((Header->DeadlineTicks &&
             HalCheckTimerExpiration(
                 CurrentTicks, Header->DeadlineReference, Header->DeadlineTicks)) ||
            Header->Finished) {
            TriggerEvent = 1;
        }
    }

    /* Check if we have any DPCs (we should probably only trigger the dispatch event if we have
     * enough DPCs, or one of the other events).*/
    if (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        TriggerEvent = 1;
    }

    /* At last, check for a quantum expiration (thread swap). */
    if (Processor->CurrentThread) {
        PsThread *CurrentThread = Processor->CurrentThread;
        if (HalCheckTimerExpiration(
                CurrentTicks, CurrentThread->ExpirationReference, CurrentThread->ExpirationTicks)) {
            TriggerEvent = 1;
        }
    }

    /* Finally, trigger an event at dispatch IRQL if we need to do anything else. */
    if (TriggerEvent) {
        HalpNotifyProcessor(Processor, 0);
    }
}
