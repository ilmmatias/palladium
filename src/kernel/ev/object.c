/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <psp.h>

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
    KeProcessor *Processor = HalGetCurrentProcessor();
    uint64_t CurrentTicks = HalGetTimerTicks();
    EvHeader *Header = Object;

    /* Enter critical section (can't let any scheduling happen here), and update the event
       queue. */
    void *Context = HalpEnterCriticalSection();
    if (Header->Finished) {
        HalpLeaveCriticalSection(Context);
        return;
    }

    /* Ignore the target timeout for already dispatched objects (we probably just want to yield
       this thread out). */
    if (!Header->Dispatched) {
        Header->DeadlineTicks = 0;
        if (Timeout) {
            Header->DeadlineReference = CurrentTicks;
            Header->DeadlineTicks = Timeout / HalGetTimerPeriod();
        }

        RtAppendDList(&HalGetCurrentProcessor()->EventQueue, &Header->ListHeader);
        Header->Dispatched = 1;
    }

    HalpLeaveCriticalSection(Context);

    if (Yield) {
        Header->Source = Processor->CurrentThread;
        PsYieldExecution(PS_YIELD_WAITING);

        /* Idle loop to make sure we won't return too early. */
        while (!Header->Finished) {
            HalpStopProcessor();
        }
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
    void *Context = HalpEnterCriticalSection();
    EvHeader *Header = Object;

    if (!Header->Dispatched) {
        HalpLeaveCriticalSection(Context);
        return;
    }

    /* No DPC dispatch happens on cancel. */
    RtUnlinkDList(&Header->ListHeader);

    if (Header->Source) {
        PsReadyThread(Header->Source);
    }

    HalpLeaveCriticalSection(Context);
}
