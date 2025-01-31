/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>

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
    KeProcessor *Processor = KeGetCurrentProcessor();

    /* Check if any waiting threads are expiring. */
    for (RtDList *ListHeader = Processor->WaitQueue.Next; ListHeader != &Processor->WaitQueue;
         ListHeader = ListHeader->Next) {
        PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        if (Thread->WaitTicks && !--Thread->WaitTicks) {
            HalpNotifyProcessor(Processor, false);
        }
    }

    /* Check if any threads have been terminated. */
    if (Processor->TerminationQueue.Next != &Processor->TerminationQueue) {
        HalpNotifyProcessor(Processor, false);
    }

    /* Update the quantum (if required). */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread && CurrentThread->ExpirationTicks && !--CurrentThread->ExpirationTicks) {
        HalpNotifyProcessor(Processor, false);
    }
}
