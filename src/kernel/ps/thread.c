/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/obp.h>
#include <kernel/psp.h>

extern KeAffinity KiIdleProcessors;

[[noreturn]] extern void PspIdleThread(void *);
[[noreturn]] extern void KiContinueSystemStartup(void *);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does the actual creation of the thread, together with creating the stack if
 *     requested to do so.
 *
 * PARAMETERS:
 *     EntryPoint - Where the thread should jump on first execution.
 *     Parameter - What to pass into the thread entry point.
 *     Stack - Which stack to use, or NULL to allocate a new one.
 *
 * RETURN VALUE:
 *     Pointer to the thread structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
static PsThread *CreateThread(void (*EntryPoint)(void *), void *Parameter, void *Stack) {
    PsThread *Thread = ObCreateObject(&ObpThreadType, MM_POOL_TAG_THREAD);
    if (!Thread) {
        return NULL;
    }

    Thread->State = PS_STATE_CREATED;
    Thread->Stack = Stack;
    if (!Thread->Stack) {
        Thread->AllocatedStack = MmAllocatePool(KE_STACK_SIZE, MM_POOL_TAG_THREAD);
        if (!Thread->AllocatedStack) {
            ObDereferenceObject(Thread);
            return NULL;
        }

        /* We're guessing that there's no need to initialize the thread context (in fact, it might
         * even be dangerous to do so) if it's an already allocated stack. */
        Thread->Stack = Thread->AllocatedStack;
        HalpInitializeContext(
            &Thread->ContextFrame, Thread->Stack, KE_STACK_SIZE, EntryPoint, Parameter);
    }

    /* The stack size (even when allocated somewhere else) should always be KE_STACK_SIZE. */
    Thread->StackLimit = Thread->Stack + KE_STACK_SIZE;
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
 *     This function sets up the specified thread inside the processor wait list (either for a delay
 *     or a wait with timeout).
 *
 * PARAMETERS:
 *     Processor - Which processor is currently active.
 *     Thread - Which thread to add to said processor's wait list.
 *     Time - How many nanoseconds to wait.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspSetupThreadWait(KeProcessor *Processor, PsThread *Thread, uint64_t Time) {
    Thread->WaitTicks = Processor->Ticks + (Time + EVP_TICK_PERIOD - 1) / EVP_TICK_PERIOD;

    /* Make sure we stop AFTER the wanted entry, as we don't have any RtInsertAfterDList, so we need
     * to use RtPushDList. */
    RtDList *ListHeader = Processor->WaitQueue.Next;
    while (ListHeader != &Processor->WaitQueue) {
        PsThread *Entry = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        if (Entry->WaitTicks >= Thread->WaitTicks) {
            break;
        }

        ListHeader = ListHeader->Next;
    }

    RtPushDList(ListHeader->Prev, &Thread->ListHeader);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new thread, followed by adding it to the execution
 *     queue.
 *
 * PARAMETERS:
 *     EntryPoint - Where the thread should jump on first execution.
 *     Parameter - What to pass into the thread entry point.
 *
 * RETURN VALUE:
 *     Pointer to the thread structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
PsThread *PsCreateThread(void (*EntryPoint)(void *), void *Parameter) {
    /* The thread creation itself can/should be done at a low IRQL. */
    PsThread *Thread = CreateThread(EntryPoint, Parameter, NULL);
    if (!Thread) {
        return NULL;
    }

    /* By default, the thread should have two refereneces: us (the scheduler), and the caller;
     * ObCreateObject already adds one reference (which would be us), so we just need to reference
     * the object again to set it up for the caller. */
    ObReferenceObject(Thread);

    /* Adding the thread to the queue requires raising the IRQL. */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
    Thread->State = PS_STATE_QUEUED;
    PspQueueThread(Thread, false);
    KeLowerIrql(OldIrql);

    return Thread;
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
    /* Raise to SYNCH (block device interrupts) and acquire the processor lock (to access the
     * queue). */
    KeRaiseIrql(KE_IRQL_SYNCH);
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    /* Make sure that we're running, because if not, how did we even get here? */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* We're mostly equivalent to SuspendThread in behaviour, so we also just use the IdleThread if
     * no other is available. */
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    PsThread *TargetThread = Processor->IdleThread;
    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
    } else {
        TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    /* We'll be panicking if SwitchContext ever returns (corrupted thread queue probably). */
    RtAppendDList(&Processor->TerminationQueue, &CurrentThread->ListHeader);
    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_TERMINATED, KE_IRQL_SYNCH);
    KeFatalError(KE_PANIC_BAD_THREAD_STATE, PS_STATE_RUNNING, PS_STATE_TERMINATED, 0, 0);
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
    /* Sleep(0) is just a YieldThread request (which did you do this instead of just calling
     * YieldThread?). */
    if (!Time) {
        PsYieldThread();
        return;
    }

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

    /* Do the wait list manipulation (that also calculates the target ticks) as early as possible
     * (so that we don't overshoot the wait too much). */
    CurrentThread->WaitObject = NULL;
    PspSetupThreadWait(Processor, CurrentThread, Time);

    /* We're mostly equivalent to SuspendThread in behaviour, so we also just use the IdleThread if
     * no other is available. */
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    PsThread *TargetThread = Processor->IdleThread;
    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
    } else {
        TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_WAITING, OldIrql);

    /* Just make sure we returned when we expected to. */
    if (Processor->Ticks < CurrentThread->WaitTicks) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, PS_STATE_RUNNING, PS_STATE_WAITING, 0, 0);
    }
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

    PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_QUEUED, OldIrql);
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
    }

    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_SUSPENDED, OldIrql);
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
    /* Clearing the affinity before creating the thread should make it go to the BSP. */
    KeInitializeAffinity(&KiIdleProcessors);

    PsThread *Thread = PsCreateThread(KiContinueSystemStartup, NULL);
    if (!Thread) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_SCHEDULER_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    /* Only the scheduler can/should hold a reference to the system startup thread. */
    ObDereferenceObject(Thread);
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
    /* As this uses the pre-existing stack, we CANNOT be jumped into from another thread until
     * HalpSwitchContext was called at least one time with us as the current thread; But this should
     * be okay, as it shouldn't happen under the normal initialization process. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    Processor->IdleThread = CreateThread(PspIdleThread, NULL, Processor->StackBase);
    if (!Processor->IdleThread) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_SCHEDULER_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    /* We're never ready or queued or anything else, always idle. */
    Processor->IdleThread->State = PS_STATE_IDLE;
}
