/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mm.h>
#include <psp.h>
#include <vid.h>

extern void KiContinueSystemStartup(void *);
extern void PspIdleThread(void *);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new thread.
 *     This won't add it to the current processor's ready list, use PsReadyThread() for that.
 *
 * PARAMETERS:
 *     EntryPoint - Where the thread should jump on first execution.
 *     Parameter - What to pass into the thread entry point.
 *
 * RETURN VALUE:
 *     Pointer to the thread structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter) {
    PsThread *Thread = MmAllocatePool(sizeof(PsThread), "Ps  ");
    if (!Thread) {
        return NULL;
    }

    Thread->Stack = MmAllocatePool(KE_STACK_SIZE, "Ps  ");
    if (!Thread->Stack) {
        MmFreePool(Thread, "Ps  ");
        return NULL;
    }

    HalpInitializeContext(&Thread->Context, Thread->Stack, KE_STACK_SIZE, EntryPoint, Parameter);

    return Thread;
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
    KeIrql OldIrql = KeAcquireSpinLock(&BestMatch->ThreadQueueLock);
    RtAppendDList(&BestMatch->ThreadQueue, &Thread->ListHeader);
    __atomic_add_fetch(&BestMatch->ThreadQueueSize, 1, __ATOMIC_SEQ_CST);
    KeReleaseSpinLock(&BestMatch->ThreadQueueLock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks the current thread for deletion, and yields out into the next thread.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void PsTerminateThread(void) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    PsThread *Thread = Processor->CurrentThread;
    Thread->Terminated = 1;
    PsYieldExecution(PS_YIELD_WAITING);
    while (1) {
        HalpStopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and enqueues the system thread. We should only be called by the BSP.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspCreateSystemThread(void) {
    HalGetCurrentProcessor()->InitialThread = PsCreateThread(KiContinueSystemStartup, NULL);
    if (!HalGetCurrentProcessor()->InitialThread) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel", "failed to create the system thread\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the idle thread for this processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspCreateIdleThread(void) {
    HalGetCurrentProcessor()->IdleThread = PsCreateThread(PspIdleThread, NULL);
    if (!HalGetCurrentProcessor()->IdleThread) {
        VidPrint(VID_MESSAGE_ERROR, "Kernel", "failed to create the idle thread\n");
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }
}
