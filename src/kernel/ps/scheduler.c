/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mm.h>
#include <psp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Cleanup routine for killed/terminated threads.
 *
 * PARAMETERS:
 *     Thread - Which thread was killed.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TerminationDpc(void *ThreadPointer) {
    PsThread *Thread = ThreadPointer;
    MmFreePool(Thread->Stack, "Ps  ");
    MmFreePool(Thread, "Ps  ");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries getting a new thread to be executed.
 *
 * PARAMETERS:
 *     Processor - Which CPU scheduler we're using.
 *     AllowInitial - Set this to 1 if we should allow scheduling the initial system thread.
 *
 * RETURN VALUE:
 *     Which thread we should run.
 *-----------------------------------------------------------------------------------------------*/
static PsThread *GetNextThread(KeProcessor *Processor, int AllowInitial) {
    /* First try getting a thread from the current processor wait list. */
    if (Processor->ThreadQueueSize) {
        KeIrql OldIrql = KeAcquireSpinLock(&Processor->ThreadQueueLock);
        RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
        if (ListHeader != &Processor->ThreadQueue) {
            __atomic_sub_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
            KeReleaseSpinLock(&Processor->ThreadQueueLock, OldIrql);
            return CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        }
        KeReleaseSpinLock(&Processor->ThreadQueueLock, OldIrql);
    }

    /* If we fail to do so, let's try stealing something from another processor. */
    for (uint32_t i = 0; i < HalpProcessorCount; i++) {
        if (HalpProcessorList[i] == Processor || !HalpProcessorList[i]->ThreadQueueSize) {
            continue;
        }

        KeIrql OldIrql = KeAcquireSpinLock(&HalpProcessorList[i]->ThreadQueueLock);
        RtDList *ListHeader = RtPopDList(&HalpProcessorList[i]->ThreadQueue);
        if (ListHeader != &HalpProcessorList[i]->ThreadQueue) {
            __atomic_sub_fetch(&HalpProcessorList[i]->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
            KeReleaseSpinLock(&HalpProcessorList[i]->ThreadQueueLock, OldIrql);
            return CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        }
        KeReleaseSpinLock(&HalpProcessorList[i]->ThreadQueueLock, OldIrql);
    }

    /* If that failed as well, then try to use the initial thread (in case this is the scheduler
     * initialization). */
    if (AllowInitial && Processor->InitialThread) {
        PsThread *Thread = Processor->InitialThread;
        Processor->InitialThread = NULL;
        return Thread;
    }

    /* Fallback to the idle thread if all else failed. */
    return Processor->IdleThread;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a DPC to handle thread termination if required.
 *
 * PARAMETERS:
 *     Processor - Which CPU scheduler we're using.
 *     Thread - Which thread to check for termination.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void CheckTermination(KeProcessor *Processor, PsThread *Thread) {
    if (Thread && Thread->Terminated) {
        EvInitializeDpc(&Thread->TerminationDpc, TerminationDpc, Thread);
        RtAppendDList(&Processor->DpcQueue, &Thread->TerminationDpc.ListHeader);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function appends the given thread back into the queue.
 *
 * PARAMETERS:
 *     Processor - Which CPU scheduler we're using.
 *     Thread - Which thread to enqueue.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void AdjustQueue(KeProcessor *Processor, PsThread *Thread) {
    if (!Thread->Terminated) {
        KeIrql OldIrql = KeAcquireSpinLock(&Processor->ThreadQueueLock);
        RtAppendDList(&Processor->ThreadQueue, &Thread->ListHeader);
        __atomic_add_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
        KeReleaseSpinLock(&Processor->ThreadQueueLock, OldIrql);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adjusts the expiration of the new current thread using the thread queue
 *     size.
 *
 * PARAMETERS:
 *     Processor - Which CPU scheduler we're using.
 *     Thread - Which thread to adjust the expiration.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void AdjustExpiration(KeProcessor *Processor, PsThread *Thread) {
    uint64_t CurrentTicks = HalGetTimerTicks();
    uint64_t ThreadQueueSize = Processor->ThreadQueueSize;
    if (Thread == Processor->IdleThread || !ThreadQueueSize) {
        /* No use in setting an expiration if we have no more threads ahead. */
        Thread->ExpirationReference = 0;
        Thread->ExpirationTicks = 0;
    } else {
        /* Otherwise just make sure we limit how low the quantum can be. */
        uint64_t ThreadQuantum = PSP_THREAD_QUANTUM / ThreadQueueSize;
        if (ThreadQuantum < PSP_THREAD_MIN_QUANTUM) {
            ThreadQuantum = PSP_THREAD_MIN_QUANTUM;
        }

        Thread->ExpirationReference = CurrentTicks;
        Thread->ExpirationTicks = ThreadQuantum / HalGetTimerPeriod();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function forcefully switches out the current thread.
 *
 * PARAMETERS:
 *     Type - Type of the yield (should we readd this thread to the queue?).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsYieldExecution(int Type) {
    /* Raise to DISPATCH (simulating the environment of the scheduler). */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    KeProcessor *Processor = HalGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;

    /* This is the only place that's allowed to switch from the non-scheduler world
     * (KiSystemStartup) into the scheduler world (KiContinueSystemStartup or PspIdleThread). */
    PsThread *TargetThread = GetNextThread(Processor, 1);
    CheckTermination(Processor, CurrentThread);
    if (Type != PS_YIELD_WAITING) {
        AdjustQueue(Processor, CurrentThread);
    }
    AdjustExpiration(Processor, TargetThread);

    Processor->CurrentThread = TargetThread;
    if (CurrentThread) {
        HalpSwitchContext(&CurrentThread->Context, &TargetThread->Context);
    } else {
        HalpSwitchContext(NULL, &TargetThread->Context);
    }

    /* And if HalpSwitchContext returned, we should be back from the thread we yielded into. */
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

    /* Don't bother with anything if PsYieldExecution still hasn't gotten us out of
     * KiSystemStartup.*/
    KeProcessor *Processor = HalGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;
    if (!CurrentThread) {
        return;
    }

    /* Don't bother with anything if the current thread still has time left til expiration. */
    uint64_t CurrentTicks = HalGetTimerTicks();
    if (CurrentThread->ExpirationTicks &&
        !HalCheckTimerExpiration(
            CurrentTicks, CurrentThread->ExpirationReference, CurrentThread->ExpirationTicks)) {
        return;
    }

    /* Don't bother with switching if we have no more threads to run; Just set the expiration to 0
     * to indicate that we want to switch asap (once we have something to do). */
    PsThread *TargetThread = GetNextThread(Processor, 0);
    if (TargetThread == Processor->IdleThread) {
        CurrentThread->ExpirationReference = 0;
        CurrentThread->ExpirationTicks = 0;
        return;
    }

    /* Otherwise adjust both threads, and switch away.*/
    CheckTermination(Processor, CurrentThread);
    AdjustQueue(Processor, CurrentThread);
    AdjustExpiration(Processor, TargetThread);
    Processor->CurrentThread = TargetThread;
    HalpSwitchContext(&CurrentThread->Context, &TargetThread->Context);
}
