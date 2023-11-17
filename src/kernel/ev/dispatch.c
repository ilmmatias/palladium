/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ev.h>
#include <halp.h>
#include <psp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function schedules out the current thread while it awaits an event to complete.
 *
 * PARAMETERS:
 *     Object - Pointer to the event object.
 *     Timeout - How many nanoseconds we should wait until we drop the event; Set this to 0 to
 *               wait indefinitely.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvWaitObject(void *Object, uint64_t Timeout) {
    HalProcessor *Processor = HalGetCurrentProcessor();
    EvHeader *Header = Object;

    /* For timers, always ignore the timeout parameter (we use their target deadline). */
    if (!Header->Deadline) {
        Header->Deadline = 0;
        if (Timeout) {
            Header->Deadline = HalGetTimerTicks() + Timeout / HalGetTimerPeriod();
        }
    }

    /* Enter critical section (can't let any scheduling happen here), and update the event
       queue (and the closest event deadline). */
    void *Context = HalpEnterCriticalSection();

    Header->Source = Processor->CurrentThread;
    Processor->ForceYield = PSP_YIELD_EVENT;
    RtAppendDList(&HalGetCurrentProcessor()->EventQueue, &Header->ListHeader);

    if (!Processor->ClosestEvent || Header->Deadline < Processor->ClosestEvent) {
        Processor->ClosestEvent = Header->Deadline;
    }

    HalpLeaveCriticalSection(Context);
    HalpNotifyProcessor(Processor, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles any pending kernel events.
 *
 * PARAMETERS:
 *     Context - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpHandleEvents(HalRegisterState *Context) {
    HalProcessor *Processor = HalGetCurrentProcessor();
    uint64_t CurrentTicks = HalGetTimerTicks();

    /* Process any pending events (they might enqueue DPCs, so we need to process them first). */
    RtDList *ListHeader = Processor->EventQueue.Next;
    while (ListHeader != &Processor->EventQueue) {
        EvHeader *Header = CONTAINING_RECORD(ListHeader, EvHeader, ListHeader);
        int Finished = 0;

        ListHeader = ListHeader->Next;

        /* Out of the deadline, for anything but timers, this will make WaitObject return an
           error. */
        if (Header->Deadline && CurrentTicks >= Header->Deadline) {
            RtUnlinkDList(&Header->ListHeader);
            Processor->ClosestEvent = 0;

            KeAcquireSpinLock(&Processor->ThreadQueueLock);
            __atomic_add_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);

            /* Boost the priority of any waiting task. */
            RtPushDList(&Processor->ThreadQueue, &Header->Source->ListHeader);
            KeReleaseSpinLock(&Processor->ThreadQueueLock);

            Finished = 1;
        }

        if (Finished && Header->Dpc) {
            RtAppendDList(&Processor->DpcQueue, &Header->Dpc->ListHeader);
        }
    }

    /* Process any pending DPCs. */
    while (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        EvDpc *Dpc = CONTAINING_RECORD(RtPopDList(&Processor->DpcQueue), EvDpc, ListHeader);
        Dpc->Routine(Dpc->Context);
    }

    /* Wrap up by calling the scheduler; It should handle initialization+retriggering the event
       handler if required. */
    PspScheduleNext(Context);
}
