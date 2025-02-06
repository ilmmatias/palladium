/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/psp.h>

extern KeAffinity KiIdleProcessors;

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
 *     This function tries to queue the specified thread under the specified processor.
 *
 * PARAMETERS:
 *     Thread - Which thread to add.
 *     Processor - Which processor to add the thread to.
 *     EventQueue - Set this to true if this thread was waiting for an event that expired or was
 *                  signaled.
 *     ForceIdle - Set this to true if we should bail out in case the thread got out of idle
 *                 before we queued the thread to it.
 *
 * RETURN VALUE:
 *     true if the thread was queued, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static bool
TryQueueThreadIn(PsThread *Thread, KeProcessor *Processor, bool EventQueue, bool ForceIdle) {
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    if (ForceIdle && !KeGetAffinityBit(&KiIdleProcessors, Processor->Number)) {
        KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
        return false;
    }

    if (EventQueue) {
        RtPushDList(&Processor->ThreadQueue, &Thread->ListHeader);
    } else {
        RtAppendDList(&Processor->ThreadQueue, &Thread->ListHeader);
    }

    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
    return true;
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
    /* We prefer to stay on the current processor. */
    if (TryQueueThreadIn(Thread, KeGetCurrentProcessor(), EventQueue, true)) {
        return;
    }

    /* Otherwise, we try to queue in the left most thread that is idle (empty queue and/or running
     * the idle thread). */
    while (true) {
        uint32_t Index = KeGetFirstAffinitySetBit(&KiIdleProcessors);
        if (Index == (uint32_t)-1) {
            break;
        } else if (TryQueueThreadIn(Thread, HalpProcessorList[Index], EventQueue, true)) {
            return;
        }
    }

    /* If all fails, we just assign the thread to the current processor. */
    TryQueueThreadIn(Thread, KeGetCurrentProcessor(), EventQueue, false);
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
    KeRaiseIrql(KE_IRQL_SYNCH);
    KeProcessor *Processor = KeGetCurrentProcessor();
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
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
        KeProcessor *Processor = KeGetCurrentProcessor();
        Processor->CurrentThread->WaitTicks =
            Processor->Ticks + (Time + EVP_TICK_PERIOD - 1) / EVP_TICK_PERIOD;
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
    KeInitializeAffinity(&KiIdleProcessors);
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
