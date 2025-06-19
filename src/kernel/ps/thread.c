/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/obp.h>
#include <kernel/psp.h>

extern KeAffinity KiIdleProcessors;

uint64_t PspGlobalThreadCount = 0;

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
        Thread->AllocatedStack = MmAllocateKernelStack();
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
 *     This function queues the specified thread under the specified processor.
 *
 * PARAMETERS:
 *     Thread - Which thread to add.
 *     Processor - Which processor to add the thread to.
 *     EventQueue - Set this to true if this thread was waiting for an event that expired or was
 *                  signaled.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void QueueThreadIn(PsThread *Thread, KeProcessor *Processor, bool EventQueue) {
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    if (EventQueue) {
        RtPushDList(&Processor->ThreadQueue, &Thread->ListHeader);
    } else {
        RtAppendDList(&Processor->ThreadQueue, &Thread->ListHeader);
    }

    __atomic_add_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELAXED);
    __atomic_add_fetch(&PspGlobalThreadCount, 1, __ATOMIC_RELAXED);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function queues the specified thread list under the specified processor.
 *
 * PARAMETERS:
 *     ThreadList - Which threads to add.
 *     ThreadCount - How many threads to add.
 *     Processor - Which processor to add the threads to.
 *     EventQueue - Set this to true if these threads were waiting for an event/signal.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void QueueThreadListIn(
    RtDList *ThreadList,
    uint64_t ThreadCount,
    KeProcessor *Processor,
    bool EventQueue) {
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    if (EventQueue) {
        RtSpliceHeadDList(&Processor->ThreadQueue, ThreadList);
    } else {
        RtSpliceTailDList(&Processor->ThreadQueue, ThreadList);
    }

    __atomic_add_fetch(&Processor->ThreadCount, ThreadCount, __ATOMIC_RELAXED);
    __atomic_add_fetch(&PspGlobalThreadCount, ThreadCount, __ATOMIC_RELAXED);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
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
    /* First, if the current inbalance isn't too bad, we want to place it in the current processor
     * (as the processor's cache will probably be more warm/have more hits for the thread if we stay
     * always on the same thread).  */
    KeProcessor *Processor = KeGetCurrentProcessor();
    uint64_t LocalThreadCount = __atomic_load_n(&Processor->ThreadCount, __ATOMIC_RELAXED) + 1;
    uint64_t GlobalThreadCount = __atomic_load_n(&PspGlobalThreadCount, __ATOMIC_RELAXED) + 1;
    if (LocalThreadCount < (GlobalThreadCount * PSP_LOAD_BALANCE_BIAS) / 100) {
        QueueThreadIn(Thread, Processor, EventQueue);
        return;
    }

    /* Otherwise, we'd rather place the thread in an idle processor; We'll just assume the processor
     * is still idle (rather than looping until we lock() an actually idle processor). */
    uint32_t Index = KeGetFirstAffinitySetBit(&KiIdleProcessors);
    if (Index != (uint32_t)-1) {
        KeProcessor *Processor = HalpProcessorList[Index];
        QueueThreadIn(Thread, Processor, EventQueue);
        return;
    }

    /* Otherwise, we fallback onto the slow path, and search for the least loaded processor (falling
     * back to the current processor if everyone is equally loaded). */
    uint32_t TargetIndex = Processor->Number;
    uint64_t TargetLoad = 0;
    for (uint32_t i = 0; i < HalpOnlineProcessorCount; i++) {
        KeProcessor *Processor = HalpProcessorList[i];
        uint64_t ThreadCount = __atomic_load_n(&Processor->ThreadCount, __ATOMIC_RELAXED);
        if (ThreadCount < TargetLoad) {
            TargetIndex = i;
            TargetLoad = ThreadCount;
        }
    }

    QueueThreadIn(Thread, HalpProcessorList[TargetIndex], EventQueue);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function distributes a set of threads amonst the online processors; We expect to be at
 *     least raised to DISPATCH, and with no processor locks acquired.
 *
 * PARAMETERS:
 *     ThreadList - Which threads to add.
 *     EventQueue - Set this to true if these threads were waiting for an event/signal.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspQueueThreadList(RtDList *ThreadList, uint64_t ThreadCount, bool EventQueue) {
    /* Before doing anything too complex, just make sure we have more than one thread to split; If
     * not, use the usual QueueThread() path. */
    if (ThreadCount == 1) {
        PspQueueThread(CONTAINING_RECORD(ThreadList->Next, PsThread, ListHeader), EventQueue);
        return;
    }

    /* First, if the current inbalance isn't too bad, we want to place it in the current processor
     * (as the processor's cache will probably be more warm/have more hits for the thread if we stay
     * always on the same thread).  */
    KeProcessor *Processor = KeGetCurrentProcessor();
    uint64_t LocalThreadCount =
        __atomic_load_n(&Processor->ThreadCount, __ATOMIC_RELAXED) + ThreadCount;
    uint64_t GlobalThreadCount =
        __atomic_load_n(&PspGlobalThreadCount, __ATOMIC_RELAXED) + ThreadCount;
    if (LocalThreadCount < (GlobalThreadCount * PSP_LOAD_BALANCE_BIAS) / 100) {
        QueueThreadListIn(ThreadList, ThreadCount, Processor, EventQueue);
        return;
    }

    /* Otherwise, we'd rather place the threads in an idle processor, as long as the inbalance from
     * doing so isn't going to become too great. */
    uint32_t Index = KeGetFirstAffinitySetBit(&KiIdleProcessors);
    if (Index != (uint32_t)-1 && ThreadCount < (GlobalThreadCount * PSP_LOAD_BALANCE_BIAS) / 100) {
        QueueThreadListIn(ThreadList, ThreadCount, HalpProcessorList[Index], EventQueue);
        return;
    }

    /* If all else fails, evenly spread all threads amongst the online processors. */
    uint64_t SplitSize = ThreadCount / HalpOnlineProcessorCount;
    uint64_t Remainder = ThreadCount % HalpOnlineProcessorCount;
    for (uint32_t i = 0; i < HalpOnlineProcessorCount; i++) {
        uint64_t GroupSize = SplitSize;
        if (Remainder) {
            GroupSize++;
            Remainder--;
        }

        /* We're assuming ThreadCount is trustable, and that RtPopDList won't return an invalid
         * value as long as within ThreadCount bounds. */
        RtDList ListHead;
        RtInitializeDList(&ListHead);
        for (uint64_t i = 0; i < GroupSize; i++) {
            RtAppendDList(&ListHead, RtPopDList(ThreadList));
        }

        QueueThreadListIn(&ListHead, GroupSize, HalpProcessorList[i], EventQueue);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the specified thread inside the processor wait list (either for a
 *     delay or a wait with timeout).
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

    /* Make sure we stop AFTER the wanted entry, as we don't have any RtInsertAfterDList, so we
     * need to use RtPushDList. */
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
 *     This function does the bulk of suspending the execution of the current thread, while
 *     setting up a new thread state.
 *
 * PARAMETERS:
 *     Processor - Current processor structure.
 *     CurrentThread - Current running thread block.
 *     NewState - Which state we should put the thread on.
 *     OldIrql - Return value of KeAcquireSpinLockAndRaiseIrql; Set this to -1 if we should
 *               acquire the lock ourselves.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspSuspendExecution(
    KeProcessor *Processor,
    PsThread *CurrentThread,
    int NewState,
    KeIrql OldIrql) {
    /* If the caller hasn't done it already, raise to SYNCH (block device interrupts) and
     * acquire the processor lock (don't let any other processors mess with us while we mess
     * with the thread queue). */
    if (OldIrql == (KeIrql)-1) {
        OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    }

    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    PsThread *TargetThread = Processor->IdleThread;

    if (ListHeader == &Processor->ThreadQueue) {
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
    } else {
        __atomic_sub_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELAXED);
        __atomic_sub_fetch(&PspGlobalThreadCount, 1, __ATOMIC_RELAXED);
        TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    PspSwitchThreads(Processor, CurrentThread, TargetThread, NewState, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and initializes a new thread, setting it up according to the flags.
 *
 * PARAMETERS:
 *     Flags - A set of flags to specify some default behaviour for the new thread. By default,
 *             if PS_CREATE_SUSPENDED isn't set, the thread will be automatically queued.
 *     EntryPoint - Where the thread should jump on first execution.
 *     Parameter - What to pass into the thread entry point.
 *
 * RETURN VALUE:
 *     Pointer to the thread structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
PsThread *PsCreateThread(uint64_t Flags, void (*EntryPoint)(void *), void *Parameter) {
    /* The thread creation itself can/should be done at a low IRQL. */
    PsThread *Thread = CreateThread(EntryPoint, Parameter, NULL);
    if (!Thread) {
        return NULL;
    }

    /* By default, the thread should have two refereneces: us (the scheduler), and the caller;
     * ObCreateObject already adds one reference (which would be us), so we just need to
     * reference the object again to set it up for the caller. */
    ObReferenceObject(Thread);

    /* If the thread was requested to be initalized in the SUSPENDED state, we're pretty much
     * done; Otherwise, we need to raise the IRQL, and queue the thread. */
    if (Flags & PS_CREATE_SUSPENDED) {
        Thread->State = PS_STATE_SUSPENDED;
    } else {
        Thread->State = PS_STATE_QUEUED;
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
        PspQueueThread(Thread, false);
        KeLowerIrql(OldIrql);
    }

    return Thread;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function marks the current thread for deletion, and yields out into the next thread.
 *
 * PARAMETERS:
 *     Thread - Which thread to terminate; Use PsGetCurrentThread() as the argument to terminate
 *              the current thread.
 *
 * RETURN VALUE:
 *     Does not return for local threads, and returns if the thread was properly terminated for
 *     remote threads.
 *-----------------------------------------------------------------------------------------------*/
bool PsTerminateThread(PsThread *Thread) {
    /* Raise to SYNCH (block device interrupts) and acquire the processor lock (to access its
     * queue). */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
    KeProcessor *CurrentProcessor = KeGetCurrentProcessor();
    KeProcessor *Processor = Thread->Processor;
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    /* For local threads, make sure no other remote processor tried to suspend or terminate us
     * after we raised the IRQL (but before we acquired the lock). */
    PsThread *CurrentThread = CurrentProcessor->CurrentThread;
    if (CurrentThread == Thread && (CurrentThread->State == PS_STATE_PENDING_SUSPEND ||
                                    CurrentThread->State == PS_STATE_PENDING_TERMINATE)) {
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return false;
    }

    /* Otherwise, local threads need to be RUNNING (it doesn't even make sense for them to reach
     * this function if they aren't running). */
    if (CurrentThread == Thread && CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* Remote threads are allowed to be in a few different states. Any other state is too unsafe
     * to mess with. */
    if (CurrentThread != Thread && Thread->State != PS_STATE_QUEUED &&
        Thread->State != PS_STATE_RUNNING && Thread->State != PS_STATE_WAITING) {
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return false;
    }

    /* For local threads, as we know we're RUNNING, we can just forcefully SwitchThreads while
     * not requeueing ourselves. */
    if (CurrentThread == Thread) {
        RtAppendDList(&Processor->TerminationQueue, &CurrentThread->ListHeader);
        PspSuspendExecution(Processor, CurrentThread, PS_STATE_TERMINATED, KE_IRQL_SYNCH);
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, PS_STATE_RUNNING, PS_STATE_TERMINATED, 0, 0);
    }

    /* For non-running remote threads, we can just unqueue and add to the termination-list; At
     * some point, the target processor should finish the clean up. */
    if (Thread->State == PS_STATE_QUEUED) {
        RtUnlinkDList(&Thread->ListHeader);
        __atomic_sub_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELAXED);
        __atomic_sub_fetch(&PspGlobalThreadCount, 1, __ATOMIC_RELAXED);
        Thread->State = PS_STATE_TERMINATED;
        RtAppendDList(&Processor->TerminationQueue, &Thread->ListHeader);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    } else if (Thread->State == PS_STATE_WAITING) {
        /* Waiting threads a bit different; They aren't running (so we don't need to use the
         * transition state), but we do need to cleanup the waiting lists attached to it. */
        Thread->State = PS_STATE_PENDING_TERMINATE;
        RtUnlinkDList(&Thread->ListHeader);
        KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

        EvHeader *Event = Thread->WaitObject;
        if (Event) {
            /* Now with the processor lock released, acquire the event lock (to modify the
             * per-event wait list). */
            KeAcquireSpinLockAtCurrentIrql(&Event->Lock);

            /* We might have been signaled right as we were acquiring the lock, otherwise, we
             * can just unlink the event header. */
            if (Thread->WaitListHeader.Prev->Next == &Thread->WaitListHeader) {
                RtUnlinkDList(&Thread->WaitListHeader);
            }

            KeReleaseSpinLockAtCurrentIrql(&Event->Lock);
        }

        KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);
        RtAppendDList(&Processor->TerminationQueue, &Thread->ListHeader);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    } else {
        Thread->State = PS_STATE_PENDING_TERMINATE;
        HalpNotifyProcessor(Processor);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function delays the execution of the current thread until at least a certain amount
 *of time has passed.
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

    /* Make sure no other remote processor tried to suspend or terminate us after we raised the
     * IRQL (but before we acquired the lock). */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread->State == PS_STATE_PENDING_SUSPEND ||
        CurrentThread->State == PS_STATE_PENDING_TERMINATE) {
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return;
    }

    /* Make sure that we're running, because if not, how did we even get here? */
    if (CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* Do the wait list manipulation (that also calculates the target ticks) as early as
     * possible (so that we don't overshoot the wait too much). */
    CurrentThread->WaitObject = NULL;
    PspSetupThreadWait(Processor, CurrentThread, Time);
    PspSuspendExecution(Processor, CurrentThread, PS_STATE_WAITING, OldIrql);

    /* Just make sure we returned when we expected to. */
    if (Processor->Ticks < CurrentThread->WaitTicks) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, PS_STATE_RUNNING, PS_STATE_WAITING, 0, 0);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries to give up the remaining quantum in the thread, and switch out to the
 *     next thread.
 *     We won't switch to the idle thread, so when the queue is empty, this function will
 *instantly return!
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

    /* Make sure no other remote processor tried to suspend or terminate us after we raised the
     * IRQL (but before we acquired the lock). */
    PsThread *CurrentThread = Processor->CurrentThread;
    if (CurrentThread->State == PS_STATE_PENDING_SUSPEND ||
        CurrentThread->State == PS_STATE_PENDING_TERMINATE) {
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return;
    }

    /* Make sure that we're running, because if not, how did we even get here? */
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
        /* If we call YieldThread on a tight loop, we need to make sure we clear the idle bit
         * once the queue isn't empty. */
        __atomic_sub_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELAXED);
        __atomic_sub_fetch(&PspGlobalThreadCount, 1, __ATOMIC_RELAXED);
        KeClearAffinityBit(&KiIdleProcessors, Processor->Number);
    }

    PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_QUEUED, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function suspends the execution of a given thread. The thread will be in the
 *suspended state until PsResumeThread is called (or the thread is terminated).
 *
 * PARAMETERS:
 *     Thread - Which thread to suspend; If you want to (for some reason) suspend the current
 *              thread, just pass PsGetCurrentThread() to this.
 *
 * RETURN VALUE:
 *     Returns true if we were to suspend the thread properly, or false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool PsSuspendThread(PsThread *Thread) {
    /* Raise to SYNCH (block device interrupts) and acquire the processor lock (to access its
     * queue). */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);
    KeProcessor *CurrentProcessor = KeGetCurrentProcessor();
    KeProcessor *Processor = Thread->Processor;
    KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);

    /* For local threads, make sure no other remote processor tried to suspend or terminate us
     * after we raised the IRQL (but before we acquired the lock). */
    PsThread *CurrentThread = CurrentProcessor->CurrentThread;
    if (CurrentThread == Thread && (CurrentThread->State == PS_STATE_PENDING_SUSPEND ||
                                    CurrentThread->State == PS_STATE_PENDING_TERMINATE)) {
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return false;
    }

    /* Local threads need to be RUNNING (otherwise, it doesn't even make sense for them to reach
     * this function). */
    if (CurrentThread == Thread && CurrentThread->State != PS_STATE_RUNNING) {
        KeFatalError(KE_PANIC_BAD_THREAD_STATE, CurrentThread->State, PS_STATE_RUNNING, 0, 0);
    }

    /* Remote threads are allowed to be either RUNNING or QUEUED. Any other state is too unsafe
     * to mess with. */
    if (CurrentThread != Thread && Thread->State != PS_STATE_QUEUED &&
        Thread->State != PS_STATE_RUNNING) {
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return false;
    }

    /* For local threads, as we know we're RUNNING, we can just forcefully SwitchThreads while
     * not requeueing ourselves. */
    if (CurrentThread == Thread) {
        PspSuspendExecution(Processor, CurrentThread, PS_STATE_SUSPENDED, OldIrql);
        return true;
    }

    /* Remote threads have two paths; If we're queued, we have literally nothing special
     * required to be done, we just pop the thread out of the queue and mark it as suspended;
     * For running threads, we need to mark them as pending suspension, and notify the remote
     * processor that they need to swap threads. */
    if (Thread->State == PS_STATE_QUEUED) {
        RtUnlinkDList(&Thread->ListHeader);
        __atomic_sub_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELAXED);
        __atomic_sub_fetch(&PspGlobalThreadCount, 1, __ATOMIC_RELAXED);
        Thread->State = PS_STATE_SUSPENDED;
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    } else {
        Thread->State = PS_STATE_PENDING_SUSPEND;
        HalpNotifyProcessor(Processor);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resumes execution and requeues a previously suspended thread.
 *
 * PARAMETERS:
 *     Thread - Which thread to resume. The thread should be on the PS_STATE_SUSPENDED state.
 *
 * RETURN VALUE:
 *     Returns true if we were to resume the thread properly, or false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool PsResumeThread(PsThread *Thread) {
    /* Raise to SYNCH (block device interrupts) before messing with the queue functions. */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_SYNCH);

    /* Don't bother with anything that isn't suspended yet. */
    if (Thread->State != PS_STATE_SUSPENDED) {
        KeLowerIrql(OldIrql);
        return false;
    }

    /* At the end, we just need to requeue the thread (just like what we do on PsCreateThread).
     */
    Thread->State = PS_STATE_QUEUED;
    PspQueueThread(Thread, false);
    KeLowerIrql(OldIrql);
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates and enqueues the system thread. We should only be called by the
 *     boot processor.
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

    PsThread *Thread = PsCreateThread(PS_CREATE_DEFAULT, KiContinueSystemStartup, NULL);
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
     * HalpSwitchContext was called at least one time with us as the current thread; But this
     * should be okay, as it shouldn't happen under the normal initialization process. */
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
