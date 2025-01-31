/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/psp.h>

[[noreturn]] extern void PspIdleThread(void *);
[[noreturn]] extern void KiContinueSystemStartup(void *);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new thread.
 *
 * PARAMETERS:
 *     EntryPoint - Where the thread should jump on first execution.
 *     Parameter - What to pass into the thread entry point.
 *
 * RETURN VALUE:
 *     Pointer to the thread structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter) {
    PsThread *Thread = MmAllocatePool(sizeof(PsThread), "PsTh");
    if (!Thread) {
        return NULL;
    }

    Thread->Stack = MmAllocatePool(KE_STACK_SIZE, "PsTh");
    if (!Thread->Stack) {
        MmFreePool(Thread, "PsTh");
        return NULL;
    }

    Thread->StackLimit = Thread->Stack + KE_STACK_SIZE;
    HalpInitializeContext(
        &Thread->ContextFrame, Thread->Stack, KE_STACK_SIZE, EntryPoint, Parameter);

    return Thread;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds a target processor and adds a thread to its queue; We expect to be at
 *     least raised to DISPATCH, and with no processor locks acquired.
 *
 * PARAMETERS:
 *     Thread - Which thread to add.
 *     EventQueue - Set this to true if this thread was waiting for an event that expired or was
 *                  signaled.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspQueueThread(PsThread *Thread, bool EventQueue) {
    KeProcessor *TargetProcessor = KeGetCurrentProcessor();
    bool NotifyProcessor = false;

    do {
        /* BSP always gets the KiContinueSystemStartup thread. */
        if (!TargetProcessor->CurrentThread) {
            break;
        }

        /* We prefer to stay in the current processor if possible. */
        if (TargetProcessor->CurrentThread && !TargetProcessor->CurrentThread->ExpirationTicks &&
            TargetProcessor->ThreadQueue.Next == &TargetProcessor->ThreadQueue) {
            NotifyProcessor = true;
            break;
        }

        /* If not possible, then we prefer any other processor that is idle. */
        for (uint32_t i = 0; i < HalpProcessorCount; i++) {
            KeProcessor *Processor = HalpProcessorList[i];
            if (Processor->CurrentThread && !Processor->CurrentThread->ExpirationTicks &&
                Processor->ThreadQueue.Next == &Processor->ThreadQueue) {
                TargetProcessor = Processor;
                NotifyProcessor = true;
                break;
            }
        }

        /* Fallback to inserting in the current processor if no one was idle. */
    } while (false);

    KeAcquireSpinLockAtCurrentIrql(&TargetProcessor->Lock);

    if (EventQueue) {
        RtPushDList(&TargetProcessor->ThreadQueue, &Thread->ListHeader);
    } else {
        RtAppendDList(&TargetProcessor->ThreadQueue, &Thread->ListHeader);
    }

    KeReleaseSpinLockAtCurrentIrql(&TargetProcessor->Lock);

    /* Finally, wake up the target if it was idle. */
    if (NotifyProcessor) {
        HalpNotifyProcessor(TargetProcessor, false);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a thread to the execution queue.
 *
 * PARAMETERS:
 *     Thread - Which thread to add.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsQueueThread(PsThread *Thread) {
    /* DISPATCH should be enough? */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
    PspQueueThread(Thread, false);
    KeLowerIrql(OldIrql);
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
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeRaiseIrql(KE_IRQL_SYNCH);
    RtAppendDList(&Processor->TerminationQueue, &Processor->CurrentThread->ListHeader);
    PsYieldThread(PS_YIELD_TYPE_UNQUEUE);
    while (true) {
        StopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function delays the execution of the current thread until at least a certain amount of
 *     time has passed.
 *
 * PARAMETERS:
 *     Time - How much time we want to sleep for (nanoseconds, the actual amount of time spent
 *            sleeping will be aligned to the timer tick period).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsDelayThread(uint64_t Time) {
    /* Don't bother adding to the wait list for any value that is too low. */
    if (Time >= EVP_TICK_PERIOD) {
        KeProcessor *Processor = KeGetCurrentProcessor();
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
        Processor->CurrentThread->WaitTicks = (Time + EVP_TICK_PERIOD - 1) / EVP_TICK_PERIOD;
        RtAppendDList(&Processor->WaitQueue, &Processor->CurrentThread->ListHeader);
        PsYieldThread(PS_YIELD_TYPE_UNQUEUE);
        KeLowerIrql(OldIrql);
    } else {
        PsYieldThread(PS_YIELD_TYPE_QUEUE);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and enqueues the system thread. We should only be called by the boot
 *     processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspCreateSystemThread(void) {
    PsThread *Thread = PsCreateThread(KiContinueSystemStartup, NULL);
    if (!Thread) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_SCHEDULER_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    /* As the scheduler is still under initialization, this should enqueue the thread in the current
     * (boot) processor. */
    PsQueueThread(Thread);
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
    KeGetCurrentProcessor()->IdleThread = PsCreateThread(PspIdleThread, NULL);
    if (!KeGetCurrentProcessor()->IdleThread) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_SCHEDULER_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }
}
