/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <halp.h>
#include <ki.h>
#include <psp.h>

extern PsThread *PspSystemThread;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a thread to the current processor queue, calling a switch event
 *     if overdue.
 *
 * PARAMETERS:
 *     Thread - Which thread to add.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsReadyThread(PsThread *Thread) {
    HalProcessor *Processor = HalGetCurrentProcessor();

    KeAcquireSpinLock(&Processor->ThreadQueueLock);
    RtAppendDList(&Processor->ThreadQueue, &Thread->ListHeader);
    KeReleaseSpinLock(&Processor->ThreadQueueLock);

    if (HalGetTimerTicks() >= Processor->CurrentThread->Expiration) {
        HalpSetEvent(0);
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
        HalpRestoreContext(Context, &Processor->CurrentThread->Context);
        return;
    }

    /* Quantum expiry, switch threads if possible. */
    uint64_t CurrentTicks = HalGetTimerTicks();
    if (CurrentTicks >= CurrentThread->Expiration) {
        PsThread *NewThread = GetNextReadyThread(Processor);
        if (!NewThread) {
            return;
        }

        NewThread->Expiration = HalGetTimerTicks() + PSP_THREAD_QUANTUM / HalGetTimerPeriod();
        Processor->CurrentThread = NewThread;

        if (CurrentThread != Processor->IdleThread) {
            KeAcquireSpinLock(&Processor->ThreadQueueLock);
            RtAppendDList(&Processor->ThreadQueue, &CurrentThread->ListHeader);
            KeReleaseSpinLock(&Processor->ThreadQueueLock);
            HalpSetEvent(PSP_THREAD_QUANTUM);
            HalpSaveContext(Context, &CurrentThread->Context);
        }

        HalpRestoreContext(Context, &NewThread->Context);
        return;
    }

    /* We're here too early, try again in a bit. */
    HalpSetEvent((CurrentThread->Expiration - CurrentTicks) * HalGetTimerPeriod());
}
