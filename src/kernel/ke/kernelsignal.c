/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given kernel (dispatch level) signal object; The initialization
 *     is done in a generic way, and the signal object can later be enqueued by any processor.
 *
 * PARAMETERS:
 *     Signal - Pointer to the signal object structure.
 *     Routine - Which function should be executed when the signal is called.
 *     Context - Some opaque data to be passed into the signal routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeInitializeKernelSignal(KeKernelSignal* Signal, void (*Routine)(void*), void* Context) {
    Signal->Routine = Routine;
    Signal->Context = Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enqueues the given signal object to be executed in the current processor
 *     whenever possible.
 *
 * PARAMETERS:
 *     Signal - Pointer to the initialized signal object structure.
 *     HighPriority - Set this to true if this signal should be executed as soon as possible.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeQueueKernelSignal(KeKernelSignal* Signal, bool HighPriority) {
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_MAX);
    KeProcessor* Processor = KeGetCurrentProcessor();

    if (HighPriority) {
        RtPushDList(&Processor->KernelSignalQueue, &Signal->ListHeader);
    } else {
        RtAppendDList(&Processor->KernelSignalQueue, &Signal->ListHeader);
    }

    /* If we're not high priority, we'll be depending on the timer interrupt to periodically clean
     * up the signal queue, otherwise, we want to trigger a dispatch interrupt ASAP! */
    KeLowerIrql(OldIrql);
    if (HighPriority) {
        HalpNotifyProcessor(Processor);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function empties the kernel signal queue for the current processor. We expect to run
 *     under IRQL==DISPATCH.
 *
 * PARAMETERS:
 *     InterruptFrame - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiProcessKernelSignalQueue(HalInterruptFrame*) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH, Irql, 0, 0, 0);
    }

    KeProcessor* Processor = KeGetCurrentProcessor();
    while (Processor->KernelSignalQueue.Next != &Processor->KernelSignalQueue) {
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_MAX);
        KeKernelSignal* Signal = CONTAINING_RECORD(
            RtPopDList(&Processor->KernelSignalQueue), KeKernelSignal, ListHeader);
        KeLowerIrql(OldIrql);
        Signal->Routine(Signal->Context);
    }
}
