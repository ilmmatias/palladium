/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given DPC object; The initialization is done in a generic way,
 *     and the DPC object can later be enqueued by any processor.
 *
 * PARAMETERS:
 *     Dpc - Pointer to the DPC object structure.
 *     Routine - Which function should be executed when the DPC is called.
 *     Context - Some opaque data to be passed into the DPC routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeInitializeDpc(KeDpc* Dpc, void (*Routine)(void*), void* Context) {
    Dpc->Routine = Routine;
    Dpc->Context = Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enqueues the given DPC object to be executed in the current processor whenever
 *     possible.
 *
 * PARAMETERS:
 *     Dpc - Pointer to the initialized DPC object structure.
 *     HighPriority - Set this to true if this DPC should be executed as soon as possible.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeQueueDpc(KeDpc* Dpc, bool HighPriority) {
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_MAX);
    KeProcessor* Processor = KeGetCurrentProcessor();

    if (HighPriority) {
        RtPushDList(&Processor->DpcQueue, &Dpc->ListHeader);
    } else {
        RtAppendDList(&Processor->DpcQueue, &Dpc->ListHeader);
    }

    /* If we're not high priority, we'll be depending on the timer interrupt to periodically clean
     * up the DPC queue, otherwise, we want to trigger a dispatch interrupt ASAP! */
    KeLowerIrql(OldIrql);
    if (HighPriority) {
        HalpNotifyProcessor(Processor);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function empties the DPC queue for the current processor. We expect to run under
 *     IRQL==DISPATCH.
 *
 * PARAMETERS:
 *     InterruptFrame - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiProcessDpcQueue(HalInterruptFrame*) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH, Irql, 0, 0, 0);
    }

    /* There is no need to hold the DPC lock while we run the DPC routine itself; Doing it like this
     * actually allows for the DPC routines to register more DPC routines (which is probably a nice
     * feature to have). */
    KeProcessor* Processor = KeGetCurrentProcessor();
    while (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_MAX);
        KeDpc* Dpc = CONTAINING_RECORD(RtPopDList(&Processor->DpcQueue), KeDpc, ListHeader);
        KeLowerIrql(OldIrql);
        Dpc->Routine(Dpc->Context);
    }
}
