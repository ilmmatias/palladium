/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles a clock event (triggers a dispatch event if neessary).
 *
 * PARAMETERS:
 *     InterruptFrame - Current interrupt data.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpHandleTimer(HalInterruptFrame *InterruptFrame) {
    KeProcessor *Processor = KeGetCurrentProcessor();

    /* Update the runtime counters. */
    Processor->Ticks++;
    if (InterruptFrame->Irql >= KE_IRQL_DISPATCH) {
        Processor->HighIrqlTicks++;
    } else if (Processor->CurrentThread != Processor->IdleThread) {
        Processor->LowIrqlTicks++;
    } else {
        Processor->IdleTicks++;
    }

    /* Check if we have any kernel signals to execute. */
    bool NotifyProcessor = false;
    if (Processor->KernelSignalQueue.Next != &Processor->KernelSignalQueue) {
        NotifyProcessor = true;
    }

    /* Check if any threads have been terminated. */
    if (Processor->TerminationQueue.Next != &Processor->TerminationQueue) {
        NotifyProcessor = true;
    }

    /* Check if the closest event has already reached its expiration tick. */
    if (Processor->Ticks >= Processor->ClosestWaitTick) {
        NotifyProcessor = true;
    }

    /* Update the quantum (if required). */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread && CurrentThread->ExpirationTicks && !--CurrentThread->ExpirationTicks) {
        NotifyProcessor = true;
    }

    /* And finally, if any of the conditions above were true, notify the current processor to run
     * the dispatch handler. */
    if (NotifyProcessor) {
        HalpNotifyProcessor(Processor);
    }
}
