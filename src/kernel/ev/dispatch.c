/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/ke.h>
#include <kernel/psp.h>

//
#include <kernel/vid.h>

extern KeAffinity KiIdleProcessors;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to wake all threads that were waiting for the given object. We should
 *     already by at DISPATCH with the event lock held.
 *
 * PARAMETERS:
 *     Header - Common event header of the object.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpWakeThreads(EvHeader *Header) {
    while (Header->WaitList.Next != &Header->WaitList) {
        RtDList *ListHeader = RtPopDList(&Header->WaitList);
        PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, WaitListHeader);

        /* Do the main checks under the processor lock; This guarantees that we'll be properly
         * synched with EvWaitForObject (and won't accidentally "see"/try to manipulate a thread in
         * the list before it enters the waiting state). */
        KeProcessor *Processor = Thread->Processor;
        KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

        if (Thread->State != PS_STATE_WAITING) {
            KeFatalError(KE_PANIC_BAD_THREAD_STATE, Thread->State, PS_STATE_WAITING, 0, 0);
        } else if (Thread->WaitTicks) {
            RtUnlinkDList(&Thread->ListHeader);
        }

        KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

        Thread->State = PS_STATE_QUEUED;
        PspQueueThread(Thread, true);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds the thread to the given object's waiting queue, and sets up the thread
 *     into an waiting state.
 *
 * PARAMETERS:
 *     Object - Which object to wait for.
 *     Timeout - Either how many ns to wait, or 0 (EV_TIMEOUT_UNLIMITED) for no timeout.
 *
 * RETURN VALUE:
 *     false if the timeout expires, true otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool EvWaitForObject(void *Object, uint64_t Timeout) {
    /* The object should always start with an EvHeader field. */
    EvHeader *Header = Object;
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Header->Lock, KE_IRQL_SYNCH);

    /* Shortcut if the event has already been signaled. */
    if (Header->Signaled) {
        KeReleaseSpinLockAndLowerIrql(&Header->Lock, OldIrql);
        return true;
    }

    /* We're about to modify the scheduler structures, lock the current processor (IRQL is already
     * high enough). */
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    /* Make sure the thread state is sane. */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* Setup the thread wait as early as possible (because it also does timeout-related
     * calculations). */
    CurrentThread->WaitObject = Object;
    RtPushDList(&Header->WaitList, &CurrentThread->WaitListHeader);
    if (Timeout != EV_TIMEOUT_UNLIMITED) {
        PspSetupThreadWait(Processor, CurrentThread, Timeout);
    } else {
        CurrentThread->WaitTicks = 0;
    }

    /* Now all we need to do on the event header has already been done. */
    KeReleaseSpinLockAtCurrentIrql(&Header->Lock);

    /* Grab either the next available thread, or the idle thread if all else fails. */
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    PsThread *TargetThread = Processor->IdleThread;
    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
    } else {
        TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_WAITING, OldIrql);

    /* Check the expiration vs the current processor ticks to know if a timeout happened. */
    if (Timeout == EV_TIMEOUT_UNLIMITED || Processor->Ticks < CurrentThread->WaitTicks) {
        return true;
    } else {
        return false;
    }
}
