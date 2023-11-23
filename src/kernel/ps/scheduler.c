/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ev.h>
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
    size_t BestMatchSize = SIZE_MAX;
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
    void *Context = HalpEnterCriticalSection();
    KeAcquireSpinLock(&BestMatch->ThreadQueueLock);
    __atomic_add_fetch(&BestMatch->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
    RtAppendDList(&BestMatch->ThreadQueue, &Thread->ListHeader);
    KeReleaseSpinLock(&BestMatch->ThreadQueueLock);
    HalpLeaveCriticalSection(Context);

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
    HalProcessor *Processor = HalGetCurrentProcessor();

    /* PspScheduleNext should forcefully "switch" threads if we're not running anything. */
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

    if (ListHeader != &Processor->ThreadQueue) {
        return CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    return Processor->ForceYield ? Processor->IdleThread : NULL;
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
    HalProcessor *Processor = HalGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;

    /* Scheduler initialization, there should be no contention yet (we're using InitialThread
       instead of the queue). */
    if (!Processor->CurrentThread) {
        Processor->CurrentThread = Processor->InitialThread;
        Processor->CurrentThread->Expiration = 0;
        HalpRestoreThreadContext(Context, &Processor->CurrentThread->Context);
        return;
    }

    /* On quantum expiry, we switch threads if possible;
       On force yield, we always switch threads (into the idle if nothing is left). */
    uint64_t CurrentTicks = HalGetTimerTicks();
    if (CurrentTicks >= CurrentThread->Expiration || Processor->ForceYield) {
        PsThread *NewThread = GetNextReadyThread(Processor);
        if (!NewThread) {
            CurrentThread->Expiration = 0;

            /* Setup a next timer tick (for events) if possible. */
            if (Processor->ClosestEvent) {
                HalpSetEvent((Processor->ClosestEvent - CurrentTicks) * HalGetTimerPeriod());
            }

            return;
        }

        if (NewThread == Processor->IdleThread) {
            NewThread->Expiration = 0;
        } else {
            uint64_t ThreadQuantum = PSP_THREAD_QUANTUM / Processor->ThreadQueueSize;
            if (ThreadQuantum < PSP_THREAD_MIN_QUANTUM) {
                ThreadQuantum = PSP_THREAD_MIN_QUANTUM;
            }

            NewThread->Expiration = HalGetTimerTicks() + ThreadQuantum / HalGetTimerPeriod();
        }

        /* Try setting up a timer tick for the closest event; If we can't, we're on the idle
           thread, and blocked by an event with no deadline (awaiting IO?) */
        uint64_t ThreadDeadline = NewThread->Expiration ? NewThread->Expiration : UINT64_MAX;
        uint64_t EventDeadline = Processor->ClosestEvent ? Processor->ClosestEvent : UINT64_MAX;

        if (NewThread->Expiration || Processor->ClosestEvent) {
            if (ThreadDeadline < EventDeadline) {
                HalpSetEvent((ThreadDeadline - CurrentTicks) * HalGetTimerPeriod());
            } else {
                HalpSetEvent((EventDeadline - CurrentTicks) * HalGetTimerPeriod());
            }
        }

        Processor->CurrentThread = NewThread;

        if (Processor->ForceYield == PSP_YIELD_EVENT) {
            __atomic_sub_fetch(&Processor->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
        } else if (CurrentThread != Processor->IdleThread) {
            KeAcquireSpinLock(&Processor->ThreadQueueLock);
            RtAppendDList(&Processor->ThreadQueue, &CurrentThread->ListHeader);
            KeReleaseSpinLock(&Processor->ThreadQueueLock);
        }

        if (CurrentThread != Processor->IdleThread) {
            HalpSaveThreadContext(Context, &CurrentThread->Context);
        }

        Processor->ForceYield = PSP_YIELD_NONE;
        HalpRestoreThreadContext(Context, &NewThread->Context);

        return;
    }

    /* We're here too early, try again in a bit. */
    uint64_t ThreadDeadline = CurrentThread->Expiration ? CurrentThread->Expiration : UINT64_MAX;
    uint64_t EventDeadline = Processor->ClosestEvent ? Processor->ClosestEvent : UINT64_MAX;
    if (CurrentThread->Expiration || Processor->ClosestEvent) {
        if (ThreadDeadline < EventDeadline) {
            HalpSetEvent((ThreadDeadline - CurrentTicks) * HalGetTimerPeriod());
        } else {
            HalpSetEvent((EventDeadline - CurrentTicks) * HalGetTimerPeriod());
        }
    }
}
