/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ev.h>
#include <halp.h>
#include <psp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for the next closest event deadline, and updates the Processor
 *     structure.
 *
 * PARAMETERS:
 *     Processor - Which processor we're currently running in.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void UpdateClosestDeadline(HalProcessor *Processor) {
    RtDList *ListHeader = Processor->EventQueue.Next;

    Processor->ClosestEvent = 0;
    while (ListHeader != &Processor->EventQueue) {
        EvHeader *Header = CONTAINING_RECORD(ListHeader, EvHeader, ListHeader);

        if (Header->Deadline &&
            (!Processor->ClosestEvent || Header->Deadline < Processor->ClosestEvent)) {
            Processor->ClosestEvent = Header->Deadline;
        }

        ListHeader = ListHeader->Next;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function dispatches the given object into the event queue.
 *     Do not use this function unless you're implementing a custom event, use EvWaitObject(s)
 *     instead.
 *
 * PARAMETERS:
 *     Object - Pointer to the event object.
 *     Timeout - How many nanoseconds we should wait until we drop the event; Set this to 0 to
 *               wait indefinitely.
 *     Yield - Set this to 1 if we should yield the thread (and only reinsert it after the event
 *             finishes).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpDispatchObject(void *Object, uint64_t Timeout, int Yield) {
    HalProcessor *Processor = HalGetCurrentProcessor();
    uint64_t CurrentTicks = HalGetTimerTicks();
    EvHeader *Header = Object;

    /* Enter critical section (can't let any scheduling happen here), and update the event
       queue (and the closest event deadline). */
    void *Context = HalpEnterCriticalSection();
    if (Header->Finished) {
        HalpLeaveCriticalSection(Context);
        return;
    }

    /* Ignore the target timeout for already dispatched objects (we probably just want to yield
       this thread out). */
    if (!Header->Dispatched) {
        Header->Deadline = 0;
        if (Timeout) {
            Header->Deadline = CurrentTicks + Timeout / HalGetTimerPeriod();
        }
    }

    if (!Header->Dispatched) {
        RtAppendDList(&HalGetCurrentProcessor()->EventQueue, &Header->ListHeader);
        Header->Dispatched = 1;
    }

    if (Yield) {
        Processor->ForceYield = PSP_YIELD_EVENT;
        Header->Source = Processor->CurrentThread;
    }

    int NewClosestEvent = 0;
    if (Header->Deadline &&
        (!Processor->ClosestEvent || Header->Deadline < Processor->ClosestEvent)) {
        Processor->ClosestEvent = Header->Deadline;
        NewClosestEvent = 1;
    }

    HalpLeaveCriticalSection(Context);

    /* Either we yield out, or we notify the current processor we have an event in X-time
       (if we're stuck without any thread to switch into). */
    if (Yield) {
        HalpNotifyProcessor(Processor, 1);
    } else if (!Processor->CurrentThread->Expiration && NewClosestEvent) {
        HalpSetEvent((Header->Deadline - CurrentTicks) * HalGetTimerPeriod());
    }
}

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
    EvpDispatchObject(Object, Timeout, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function cancels a previously dispatched object. This will PsReadyThread() if
 *     required.
 *
 * PARAMETERS:
 *     Object - Pointer to the event object.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvCancelObject(void *Object) {
    HalProcessor *Processor = HalGetCurrentProcessor();
    void *Context = HalpEnterCriticalSection();
    EvHeader *Header = Object;

    if (!Header->Dispatched) {
        HalpLeaveCriticalSection(Context);
        return;
    }

    /* No DPC dispatch happens on cancel. */
    RtUnlinkDList(&Header->ListHeader);
    UpdateClosestDeadline(Processor);

    if (Header->Source) {
        PsReadyThread(Header->Source);
    }

    HalpLeaveCriticalSection(Context);
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
        ListHeader = ListHeader->Next;

        /* Out of the deadline, for anything but timers, this will make WaitObject return an
           error. */
        if (Header->Deadline && CurrentTicks >= Header->Deadline) {
            Header->Finished = 1;
        }

        if (Header->Finished) {
            Header->Dispatched = 0;
            RtUnlinkDList(&Header->ListHeader);

            if (Header->Source) {
                /* Boost the priority of the waiting task, and insert it back. */
                KeAcquireSpinLock(&Processor->ThreadQueueLock);
                __atomic_add_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
                RtPushDList(&Processor->ThreadQueue, &Header->Source->ListHeader);
                KeReleaseSpinLock(&Processor->ThreadQueueLock);
            }

            if (Header->Dpc) {
                RtAppendDList(&Processor->DpcQueue, &Header->Dpc->ListHeader);
            }

            UpdateClosestDeadline(Processor);
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
