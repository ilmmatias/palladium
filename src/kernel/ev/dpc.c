/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ev.h>
#include <halp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given DPC context struct.
 *     Do not try manually initializing the struct, there's no guarantee its fields will stay the
 *     same across different kernel revisions!
 *
 * PARAMETERS:
 *     Dpc - DPC context struct.
 *     Routine - What should be executed.
 *     Context - Context for the routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvInitializeDpc(EvDpc *Dpc, void (*Routine)(void *Context), void *Context) {
    Dpc->Routine = Routine;
    Dpc->Context = Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a previously initialized DPC to this processor's list.
 *
 * PARAMETERS:
 *     Dpc - DPC context struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvDispatchDpc(EvDpc *Dpc) {
    void *Context = HalpEnterCriticalSection();
    RtAppendDList(&HalGetCurrentProcessor()->DpcQueue, &Dpc->ListHeader);
    HalpLeaveCriticalSection(Context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles dispatching any pending events in the processor queue. We expect to
 *     already be at the DISPATCH IRQL.
 *
 * PARAMETERS:
 *     None that we make use of.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpProcessQueue(HalInterruptFrame *) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    uint64_t CurrentTicks = HalGetTimerTicks();

    if (KeGetIrql() != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH);
    }

    /* Process any pending events (they might enqueue DPCs, so we need to process them
     * first). */
    RtDList *ListHeader = Processor->EventQueue.Next;
    while (ListHeader != &Processor->EventQueue) {
        EvHeader *Header = CONTAINING_RECORD(ListHeader, EvHeader, ListHeader);
        ListHeader = ListHeader->Next;

        /* Out of the deadline, for anything but timers, this will make WaitObject return an
           error. */
        if (Header->DeadlineTicks &&
            HalCheckTimerExpiration(
                CurrentTicks, Header->DeadlineReference, Header->DeadlineTicks)) {
            Header->Finished = 1;
        }

        if (Header->Finished) {
            Header->Dispatched = 0;
            RtUnlinkDList(&Header->ListHeader);

            if (Header->Source) {
                /* Boost the priority of the waiting task, and insert it back. */
                KeIrql OldIrql = KeAcquireSpinLock(&Processor->ThreadQueueLock);
                RtPushDList(&Processor->ThreadQueue, &Header->Source->ListHeader);
                __atomic_add_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
                KeReleaseSpinLock(&Processor->ThreadQueueLock, OldIrql);
            }

            if (Header->Dpc) {
                RtAppendDList(&Processor->DpcQueue, &Header->Dpc->ListHeader);
            }
        }
    }

    /* Process any pending DPCs. */
    while (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        EvDpc *Dpc = CONTAINING_RECORD(RtPopDList(&Processor->DpcQueue), EvDpc, ListHeader);
        Dpc->Routine(Dpc->Context);
    }
}
