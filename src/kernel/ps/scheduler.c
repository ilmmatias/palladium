/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <halp.h>
#include <ki.h>
#include <psp.h>

extern PsThread *PspSystemThread;

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
    RtSList *ListHeader = HalpProcessorListHead.Next;
    uint64_t BestMatchSize = UINT64_MAX;
    HalProcessor *BestMatch = NULL;

    while (ListHeader) {
        HalProcessor *Processor = CONTAINING_RECORD(ListHeader, HalProcessor, ListHeader);

        /* Unless the processor list is somehow NULL, we should be TRUE in the if below at least
           once. */
        if (!BestMatch || Processor->ThreadQueueSize < BestMatchSize) {
            BestMatchSize = Processor->ThreadQueueSize;
            BestMatch = Processor;
        }

        ListHeader = ListHeader->Next;
    }

    /* Now we're forced to lock the processor queue (there was no need up until now, as we were
       only reading). */
    KeAcquireSpinLock(&BestMatch->ThreadQueueLock);
    __atomic_add_fetch(&BestMatch->ThreadQueueSize, 1, __ATOMIC_ACQUIRE);
    RtAppendDList(&BestMatch->ThreadQueue, &Thread->ListHeader);
    KeReleaseSpinLock(&BestMatch->ThreadQueueLock);

    /* PspHandleEvent uses Expiration=0 as a sign that we need to switch asap (we were out of
       work). */
    if (!BestMatch->CurrentThread->Expiration) {
        HalpNotifyProcessor(BestMatch);
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
    HalProcessor *Processor = HalGetCurrentProcessor();

    /* PspHandleEvent should forcefully "switch" threads if we're not running anything. */
    Processor->CurrentThread = NULL;
    Processor->InitialThread = IsBsp ? PspSystemThread : Processor->IdleThread;

    HalpSetEvent(0);
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
static PsThread *GetNextReadyThread(HalProcessor *Processor) {
    KeAcquireSpinLock(&Processor->ThreadQueueLock);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    KeReleaseSpinLock(&Processor->ThreadQueueLock);
    return ListHeader == &Processor->ThreadQueue
               ? NULL
               : CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles any pending kernel events.
 *
 * PARAMETERS:
 *     Context - Current processor state.
 *
 * RETURN VALUE:
 *     None..
 *-----------------------------------------------------------------------------------------------*/
void PspHandleEvent(HalRegisterState *Context) {
    HalProcessor *Processor = HalGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;

    /* Scheduler initialization, there should be no contention yet (we're using InitialThread
       instead of the queue). */
    if (!Processor->CurrentThread) {
        Processor->CurrentThread = Processor->InitialThread;
        Processor->CurrentThread->Expiration = 0;
        HalpRestoreContext(Context, &Processor->CurrentThread->Context);
        return;
    }

    /* Quantum expiry, switch threads if possible. */
    uint64_t CurrentTicks = HalGetTimerTicks();
    if (CurrentTicks >= CurrentThread->Expiration) {
        PsThread *NewThread = GetNextReadyThread(Processor);
        if (!NewThread) {
            CurrentThread->Expiration = 0;
            return;
        }

        uint64_t ThreadQuantum = PSP_THREAD_QUANTUM / Processor->ThreadQueueSize;
        if (ThreadQuantum < PSP_THREAD_MIN_QUANTUM) {
            ThreadQuantum = PSP_THREAD_MIN_QUANTUM;
        }

        NewThread->Expiration = HalGetTimerTicks() + ThreadQuantum / HalGetTimerPeriod();
        Processor->CurrentThread = NewThread;
        HalpSetEvent(ThreadQuantum);

        if (CurrentThread != Processor->IdleThread) {
            KeAcquireSpinLock(&Processor->ThreadQueueLock);
            RtAppendDList(&Processor->ThreadQueue, &CurrentThread->ListHeader);
            KeReleaseSpinLock(&Processor->ThreadQueueLock);
            HalpSaveContext(Context, &CurrentThread->Context);
        }

        HalpRestoreContext(Context, &NewThread->Context);
        return;
    }

    /* We're here too early, try again in a bit. */
    HalpSetEvent((CurrentThread->Expiration - CurrentTicks) * HalGetTimerPeriod());
}
