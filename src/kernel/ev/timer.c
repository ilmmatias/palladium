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

    /* Check if we have any DPCs to execute. */
    if (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        HalpNotifyProcessor(Processor);
    }

    /* Check if any threads have been terminated. */
    if (Processor->TerminationQueue.Next != &Processor->TerminationQueue) {
        HalpNotifyProcessor(Processor);
    }

    /* Check if we have any waiting threads to check under DISPATCH. */
    if (Processor->WaitQueue.Next != &Processor->WaitQueue) {
        HalpNotifyProcessor(Processor);
    }

    /* Update the quantum (if required). */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread && CurrentThread->ExpirationTicks && !--CurrentThread->ExpirationTicks) {
        HalpNotifyProcessor(Processor);
    }
}
