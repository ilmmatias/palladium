/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given kernel (dispatch level) asynchronous work object; The
 *     initialization is done in a generic way, and the work object can later be enqueued by/into
 *     any processor.
 *
 * PARAMETERS:
 *     Work - Pointer to the work object structure.
 *     Routine - Which function should be executed when the work is called.
 *     Context - Some opaque data to be passed into the work routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeInitializeWork(KeWork* Work, void (*Routine)(void*), void* Context) {
    Work->Routine = Routine;
    Work->Context = Context;
    Work->Queued = false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enqueues the given work object to be executed in the current processor
 *     whenever possible.
 *
 * PARAMETERS:
 *     Work - Pointer to the initialized work object structure.
 *     HighPriority - Set this to true if this work should be executed as soon as possible.
 *
 * RETURN VALUE:
 *     true if we queued successfully, or false if another processor/thread already queued this
 *     object.
 *-----------------------------------------------------------------------------------------------*/
bool KeQueueWork(KeWork* Work, bool HighPriority) {
    bool ExpectedValue = false;
    if (!__atomic_compare_exchange_n(
            &Work->Queued, &ExpectedValue, true, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
        return false;
    }

    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_MAX);
    KeProcessor* Processor = KeGetCurrentProcessor();

    if (HighPriority) {
        RtPushDList(&Processor->WorkQueue, &Work->ListHeader);
    } else {
        RtAppendDList(&Processor->WorkQueue, &Work->ListHeader);
    }

    /* If we're not high priority, we'll be depending on the timer interrupt to periodically clean
     * up the work queue, otherwise, we want to trigger a dispatch interrupt ASAP! */
    KeLowerIrql(OldIrql);
    if (HighPriority) {
        HalpNotifyProcessor(Processor, KE_IRQL_DISPATCH);
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function empties the kernel work queue for the current processor. We expect to run
 *     under IRQL==DISPATCH.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiProcessWorkQueue(void) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_EQUAL, KE_IRQL_DISPATCH, Irql, 0, 0);
    }

    KeProcessor* Processor = KeGetCurrentProcessor();
    while (Processor->WorkQueue.Next != &Processor->WorkQueue) {
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_MAX);
        KeWork* Work = CONTAINING_RECORD(RtPopDList(&Processor->WorkQueue), KeWork, ListHeader);
        KeLowerIrql(OldIrql);
        __atomic_store_n(&Work->Queued, false, __ATOMIC_RELEASE);
        Work->Routine(Work->Context);
    }
}
