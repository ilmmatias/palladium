/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/ob.h>
#include <kernel/psp.h>

[[noreturn]] extern void PspIdleThread(void *);

static volatile uint64_t InitialiazationBarrier = 0;

KeAffinity KiIdleProcessors;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function forcefully switches out into either the system (for the BSP) or the idle (for
 *     the APs) thread, finishing the scheduler initialization.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void PspInitializeScheduler(void) {
    /* Wait until all processors finished the early initialzation stage. */
    KeSynchronizeProcessors(&InitialiazationBarrier);

    /* All processors start execution on the idle thread, which should switch into the initial
     * thread on the BSP. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    Processor->IdleThread->State = PS_STATE_RUNNING;
    Processor->CurrentThread = Processor->IdleThread;
    PspIdleThread(NULL);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to give up the remaining quantum in the thread, and switch out to the
 *     next thread.
 *     We won't switch to the idle thread, so when the queue is empty, this function will instantly
 *     return!
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsYieldThread(void) {
    /* Raise to SYNCH (block device interrupts) and acquire the processor lock (to access the
     * queue). */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    /* Make sure that we're running, because if not, how did we even get here? */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* Check if we have any thread to switch into; Unlike SuspendThread, we act like IdleThread
     * isn't a thing. */
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return;
    } else {
        /* If we call YieldThread on a tight loop, we need to make sure we clear the idle bit once
         * the queue isn't empty. */
        KeClearAffinityBit(&KiIdleProcessors, Processor->Number);
    }

    /* Mark the newly chosen target as the current one. */
    PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    TargetThread->State = PS_STATE_RUNNING;
    TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
    Processor->CurrentThread = TargetThread;
    Processor->StackBase = TargetThread->Stack;
    Processor->StackLimit = TargetThread->StackLimit;

    /* Mark the old thread as doing a context switch (then requeue it, just make sure the lock isn't
     * held when requeueing it). */
    CurrentThread->State = PS_STATE_QUEUED;
    __atomic_store_n(&CurrentThread->ContextFrame.Busy, 0x01, __ATOMIC_SEQ_CST);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
    PspQueueThread(CurrentThread, false);

    /* Swap into the new thread; We should be back to lower the IRQL after another context switches
     * back into us. */
    HalpSwitchContext(&CurrentThread->ContextFrame, &TargetThread->ContextFrame);
    KeLowerIrql(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function suspends the execution of the current thread; This is a temporary test
 *     function, in the future, the main function of this should be suspending other threads instead
 *     of the current thread.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsSuspendThread(void) {
    /* Raise to SYNCH (block device interrupts) and acquire the processor lock (to access the
     * queue). */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    /* Make sure that we're running, because if not, how did we even get here? */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* Unlike YieldThread, we do use the IdleThread if no other thread is available in the queue. */
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    PsThread *TargetThread = Processor->IdleThread;
    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
    } else {
        TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
    }

    /* Mark the newly chosen target as the current one. */
    TargetThread->State = PS_STATE_RUNNING;
    Processor->CurrentThread = TargetThread;
    Processor->StackBase = TargetThread->Stack;
    Processor->StackLimit = TargetThread->StackLimit;

    /* Mark the old thread as doing a context switch, and then do said context switch; We should be
     * back after someone resumes us, except that TODO: no one can resume us yet. */
    CurrentThread->State = PS_STATE_SUSPENDED;
    __atomic_store_n(&CurrentThread->ContextFrame.Busy, 0x01, __ATOMIC_SEQ_CST);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
    HalpSwitchContext(&CurrentThread->ContextFrame, &TargetThread->ContextFrame);
    KeLowerIrql(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles switching the current thread if required. We expect to already be at
 *     the DISPATCH IRQL.
 *
 * PARAMETERS:
 *     InterruptFrame - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspProcessQueue(HalInterruptFrame *) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH, Irql, 0, 0, 0);
    }

    /* We shouldn't have anything to do if the initial thread still isn't running. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;
    if (!CurrentThread) {
        return;
    }

    /* Cleanup any threads that have terminated (they shouldn't be the current thread anymore);
     * This needs to be before raising to SYNCH (because MmFreePool expects the IRQL to be
     * <=DISPATCH). */
    while (true) {
        RtDList *ListHeader = RtPopDList(&Processor->TerminationQueue);
        if (ListHeader == &Processor->TerminationQueue) {
            break;
        }

        PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        if (Thread->State != PS_STATE_TERMINATED) {
            KeFatalError(KE_PANIC_BAD_THREAD_STATE, Thread->State, PS_STATE_TERMINATED, 0, 0);
        }

        ObDereferenceObject(Thread, "PsTh");
    }

    /* Requeue any waiting threads that have expired (this can also be done at DISPATCH). */
    for (RtDList *ListHeader = Processor->WaitQueue.Next; ListHeader != &Processor->WaitQueue;) {
        PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        ListHeader = ListHeader->Next;

        if (Thread->State != PS_STATE_WAITING) {
            KeFatalError(KE_PANIC_BAD_THREAD_STATE, Thread->State, PS_STATE_WAITING, 0, 0);
        } else if (Processor->Ticks < Thread->WaitTicks) {
            continue;
        }

        Thread->State = PS_STATE_QUEUED;
        RtUnlinkDList(&Thread->ListHeader);
        PspQueueThread(Thread, true);
    }

    /* We shouldn't have anything left to do if we haven't expired yet (or if we're the idle
     * thread). */
    if (CurrentThread->ExpirationTicks || CurrentThread == Processor->IdleThread) {
        return;
    }

    /* Now we can raise to SYNCH (block device interrupts) and acquire the processor lock (don't let
     * any other processors mess with us while we mess with the thread queue). */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);

    /* We won't enter idle through here (as we're not forced to), so if there was nothing, just keep
     * on executing the current thread. */
    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return;
    } else {
        KeClearAffinityBit(&KiIdleProcessors, Processor->Number);
    }

    /* Mark the newly chosen target as the current one. */
    PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    TargetThread->State = PS_STATE_RUNNING;
    TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
    Processor->CurrentThread = TargetThread;
    Processor->StackBase = TargetThread->Stack;
    Processor->StackLimit = TargetThread->StackLimit;

    /* Mark it the old thread as doing a context switch (then requeue it, just make sure the lock
     * isn't held when requeueing it). */
    CurrentThread->State = PS_STATE_QUEUED;
    __atomic_store_n(&CurrentThread->ContextFrame.Busy, 0x01, __ATOMIC_SEQ_CST);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
    PspQueueThread(CurrentThread, false);

    /* Swap into the new thread; We should be back to lower the IRQL after another context switches
     * back into us. */
    HalpSwitchContext(&CurrentThread->ContextFrame, &TargetThread->ContextFrame);
    KeLowerIrql(OldIrql);
}
