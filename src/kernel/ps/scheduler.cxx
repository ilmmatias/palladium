/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <ki.h>
#include <mm.h>
#include <psp.h>

#include <cxx/lock.hxx>

extern "C" {
extern PsThread *PspSystemThread;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a thread to a processor queue, calling a switch event if overdue.
 *
 * PARAMETERS:
 *     Thread - Which thread to add.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsReadyThread(PsThread *Thread) {
    /* Always try and add the thread to the least full queue. */
    size_t BestMatchSize = SIZE_MAX;
    KeProcessor *BestMatch = NULL;

    for (uint32_t i = 0; i < HalpProcessorCount; i++) {
        /* Unless the processor list is somehow NULL, we should be TRUE in the if below at least
           once. */
        if (!BestMatch || HalpProcessorList[i]->ThreadQueueSize < BestMatchSize) {
            BestMatchSize = HalpProcessorList[i]->ThreadQueueSize;
            BestMatch = HalpProcessorList[i];
        }
    }

    /* Now we're forced to lock the processor queue (there was no need up until now, as we were
       only reading). */
    SpinLockGuard Guard(&BestMatch->ThreadQueueLock);
    __atomic_add_fetch(&BestMatch->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
    RtAppendDList(&BestMatch->ThreadQueue, &Thread->ListHeader);
    Guard.Release();

    /* PspScheduleNext uses Expiration=0 as a sign that we need to switch asap (we were out of
       work). */
    if (!BestMatch->CurrentThread->Expiration) {
        HalpNotifyProcessor(BestMatch, 0);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the thread queue for the current processor, and switches into
 *     either the idle thread (for APs) or the system thread (for the BSP).
 *
 * PARAMETERS:
 *     IsBsp - Set this to 1 if we're initializing the BSP, or to 0 for the APs.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspInitializeScheduler(int IsBsp) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    /* PspScheduleNext should forcefully "switch" threads if we're not running anything. */
    Processor->CurrentThread = NULL;
    Processor->InitialThread = IsBsp ? PspSystemThread : Processor->IdleThread;
    HalpNotifyProcessor(Processor, 1);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the next highest priority thread that can be executed.
 *
 * PARAMETERS:
 *     Processor - Which CPU scheduler we're using.
 *
 * RETURN VALUE:
 *     Which thread to execute next.
 *-----------------------------------------------------------------------------------------------*/
static PsThread *GetNextReadyThread(KeProcessor *Processor) {
    SpinLockGuard Guard(&Processor->ThreadQueueLock);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    Guard.Release();

    if (ListHeader != &Processor->ThreadQueue) {
        return CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    return Processor->ForceYield ? Processor->IdleThread : NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Cleanup routine for killed/terminated threads; We're forced to use a DPC (+this method), as
 *     PspScheduleNext runs in the context of the terminated thread.
 *
 * PARAMETERS:
 *     Thread - Which thread was killed.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void TerminationDpc(PsThread *Thread) {
    MmFreePool(Thread->Stack, "Ps  ");
    MmFreePool(Thread, "Ps  ");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries scheduling and executing the next available thread.
 *
 * PARAMETERS:
 *     Context - Current processor state.
 *
 * RETURN VALUE:
 *     None..
 *-----------------------------------------------------------------------------------------------*/
void PspScheduleNext(HalRegisterState *Context) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;

    /* Scheduler initialization, there should be no contention yet (we're using InitialThread
     * instead of the queue). */
    if (!Processor->CurrentThread) {
        Processor->CurrentThread = Processor->InitialThread;
        Processor->CurrentThread->Expiration = 0;
        HalpRestoreThreadContext(Context, &Processor->CurrentThread->Context);
        return;
    }

    if (Processor->CurrentThread->Terminated) {
        Processor->ForceYield = PSP_YIELD_EVENT;
    }

    /* On quantum expiry, we switch threads if possible;
     * On force yield, we always switch threads (into the idle if nothing is left). */
    uint64_t CurrentTicks = HalGetTimerTicks();
    if (CurrentTicks < CurrentThread->Expiration && !Processor->ForceYield) {
        return;
    }

    PsThread *NewThread = GetNextReadyThread(Processor);
    if (!NewThread) {
        return;
    }

    if (NewThread == Processor->IdleThread) {
        NewThread->Expiration = 0;
    } else {
        uint64_t ThreadQuantum = PSP_THREAD_QUANTUM / Processor->ThreadQueueSize;
        if (ThreadQuantum < PSP_THREAD_MIN_QUANTUM) {
            ThreadQuantum = PSP_THREAD_MIN_QUANTUM;
        }

        NewThread->Expiration = CurrentTicks + ThreadQuantum / HalGetTimerPeriod();
    }

    /* Thread cleanup cannot be done at the moment (we're still on the thread context!),
     * we need to enqueue a DPC if we yielded out because of PsTerminateThread. */
    if (Processor->CurrentThread->Terminated) {
        EvInitializeDpc(
            &Processor->CurrentThread->TerminationDpc,
            (void (*)(void *))TerminationDpc,
            Processor->CurrentThread);
        RtAppendDList(&Processor->DpcQueue, &Processor->CurrentThread->TerminationDpc.ListHeader);
    }

    Processor->CurrentThread = NewThread;

    if (Processor->ForceYield == PSP_YIELD_EVENT) {
        __atomic_sub_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
    } else if (CurrentThread != Processor->IdleThread) {
        SpinLockGuard Guard(&Processor->ThreadQueueLock);
        RtAppendDList(&Processor->ThreadQueue, &CurrentThread->ListHeader);
    }

    Processor->ForceYield = PSP_YIELD_NONE;

    if (CurrentThread != Processor->IdleThread) {
        HalpSaveThreadContext(Context, &CurrentThread->Context);
    }

    HalpRestoreThreadContext(Context, &NewThread->Context);
}
