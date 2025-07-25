/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/ob.h>
#include <kernel/psp.h>

[[noreturn]] extern void PspIdleThread(void *);
extern uint64_t PspGlobalThreadCount;

static volatile uint64_t InitialiazationBarrier = 0;

KeAffinity KiIdleProcessors;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function forcefully switches out into either the system (for the BSP) or the idle (for
 *     the APs) thread, finishing the scheduler initialization.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void PspInitializeScheduler(void) {
    /* Wait until all processors finished the early initialzation stage. */
    KeSynchronizeProcessors(&InitialiazationBarrier);

    /* All processors start execution on the idle thread, which should switch into the initial
     * thread on the BSP. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    Processor->IdleThread->State = PS_STATE_RUNNING;
    Processor->IdleThread->Processor = Processor;
    Processor->CurrentThread = Processor->IdleThread;
    PspIdleThread(NULL);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function compares two threads in the wait tree based on their expiration tick.
 *
 * PARAMETERS:
 *     FirstStruct - The node currently in the tree.
 *     SecondStruct - The node we're trying to insert/find.
 *
 * RETURN VALUE:
 *     Either EQUAL if the threads are equal, or LEFT/RIGHT depending on the insertion point.
 *-----------------------------------------------------------------------------------------------*/
RtAvlCompareResult PspCompareWaitThreads(RtAvlNode *FirstStruct, RtAvlNode *SecondStruct) {
    PsThread *FirstThread = CONTAINING_RECORD(FirstStruct, PsThread, WaitTreeNode);
    PsThread *SecondThread = CONTAINING_RECORD(SecondStruct, PsThread, WaitTreeNode);

    if (FirstThread->WaitTicks > SecondThread->WaitTicks) {
        return RT_AVL_COMPARE_RESULT_LEFT;
    } else if (FirstThread->WaitTicks < SecondThread->WaitTicks) {
        return RT_AVL_COMPARE_RESULT_RIGHT;
    }

    /* Do a tiebreaker with the thread struct addresses themselves, as we should be able to add
     * multiple threads with the same expiration (as long as the threads themselves are
     * different). */
    if ((uintptr_t)FirstThread > (uintptr_t)SecondThread) {
        return RT_AVL_COMPARE_RESULT_LEFT;
    } else if ((uintptr_t)FirstThread < (uintptr_t)SecondThread) {
        return RT_AVL_COMPARE_RESULT_RIGHT;
    }

    return RT_AVL_COMPARE_RESULT_EQUAL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes a context switch of the specified type between the current and
 *     specified threads. For QUEUED switches, the old/current thread is automatically rescheduled.
 *     This should be called at SYNCH with the processor lock held.
 *
 * PARAMETERS:
 *     Processor - Processor that's doing the context switch.
 *     CurrentThread - Thread running in said processor up until the switch.
 *     TargetThread - Which thread was chosen for execution.
 *     Type - Type of the context switch.
 *     OldIrql - Which IRQL to lower into after the context switch returns.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspSwitchThreads(
    KeProcessor *Processor,
    PsThread *CurrentThread,
    PsThread *TargetThread,
    uint8_t Type,
    KeIrql OldIrql) {
    /* Idle thread always has expiration 0 and state IDLE. */
    if (TargetThread != Processor->IdleThread) {
        TargetThread->State = PS_STATE_RUNNING;
        TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
        TargetThread->Processor = Processor;
    }

    /* Mark the newly chosen target as the current one. */
    Processor->CurrentThread = TargetThread;
    Processor->StackBase = TargetThread->Stack;
    Processor->StackLimit = TargetThread->StackLimit;

    /* We only want to reschedule/requeue the old thread in case of a "normal" context switch (yield
     * or quantum expiration), for anything else, we just set as busy and skip the requeue. */
    CurrentThread->State = Type;
    __atomic_store_n(&CurrentThread->ContextFrame.Busy, 0x01, __ATOMIC_RELEASE);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

    if (Type == PS_STATE_QUEUED) {
        PspQueueThread(CurrentThread, false);
    }

    /* Swap into the new thread; We should be back into the old thread when HalpSwitchContext
     * returns. */
    HalpSwitchContext(&CurrentThread->ContextFrame, &TargetThread->ContextFrame);

    /* Just check if any alerts have been queued for this thread; If so, lower to SIGNAL
     * and process them first. */
    if (CurrentThread->AlertList.Next) {
        KeLowerIrql(KE_IRQL_ALERT);
        PspProcessAlertQueue();
    }

    /* At the end of everything, lower back to the original IRQL. */
    KeLowerIrql(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles switching the current thread if required. We expect to already be at
 *     the DISPATCH IRQL.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspProcessQueue(void) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_EQUAL, KE_IRQL_DISPATCH, Irql, 0, 0);
    }

    /* We shouldn't have anything to do if the initial thread still isn't running. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    PsThread *CurrentThread = Processor->CurrentThread;
    if (!CurrentThread) {
        return;
    }

    /* Cleanup any threads that have terminated (they shouldn't be the current thread anymore);
     * This needs to be before raising to SYNCH (because MmFreePool expects the IRQL to be
     * <=DISPATCH). */
    if (Processor->TerminationQueue.Next) {
        while (true) {
            RtDList *ListHeader = RtPopDList(&Processor->TerminationQueue);
            if (ListHeader == &Processor->TerminationQueue) {
                break;
            }

            PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
            if (Thread->State != PS_STATE_TERMINATED) {
                KeFatalError(KE_PANIC_BAD_THREAD_STATE, Thread->State, PS_STATE_TERMINATED, 0, 0);
            }

            ObDereferenceObject(Thread);
        }
    }

    /* Requeue any waiting threads that have expired (this can also be done at DISPATCH). */
    if (Processor->Ticks >= Processor->ClosestWaitTick) {
        while (true) {
            /* Hold the processor lock while modifying the per-processor wait list. */
            KeAcquireSpinLockAtCurrentIrql(&Processor->Lock);
            RtAvlNode *Node = RtLookupByIndexAvlTree(&Processor->WaitTree, 0);
            if (!Node) {
                KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
                break;
            }

            /* The list should be ordered, so we're the closest expiration; If we didn't expire yet,
             * no one ahead of us did. */
            PsThread *Thread = CONTAINING_RECORD(Node, PsThread, WaitTreeNode);
            if (Thread->State != PS_STATE_WAITING) {
                KeFatalError(KE_PANIC_BAD_THREAD_STATE, Thread->State, PS_STATE_WAITING, 0, 0);
            } else if (Processor->Ticks < Thread->WaitTicks) {
                Processor->ClosestWaitTick = Thread->WaitTicks;
                KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);
                break;
            }

            RtRemoveAvlTree(&Processor->WaitTree, &Thread->WaitTreeNode);
            KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

            /* Shortcut for DelayThread(), we always want to queue the thread in that case. */
            EvHeader *Event = Thread->WaitObject;
            if (!Event) {
                Thread->State = PS_STATE_QUEUED;
                PspQueueThread(Thread, true);
                continue;
            }

            do {
                /* Now with the processor lock released, acquire the event lock (to modify the
                 * per-event wait list). */
                KeAcquireSpinLockAtCurrentIrql(&Event->Lock);

                /* Is this the best way of checking if we're still linked? Not sure, maybe we should
                 * add some other signal to say we're still inserted? */
                if (Thread->WaitListHeader.Prev->Next != &Thread->WaitListHeader) {
                    KeReleaseSpinLockAtCurrentIrql(&Event->Lock);
                    break;
                }

                RtUnlinkDList(&Thread->WaitListHeader);
                KeReleaseSpinLockAtCurrentIrql(&Event->Lock);

                /* Now that we know the event didn't get signaled just as we reached a timeout (and
                 * that can't happen anymore because we already unlinked), we can requeue the
                 * thread. */
                Thread->State = PS_STATE_QUEUED;
                PspQueueThread(Thread, true);
            } while (false);
        }

        if (!RtQuerySizeAvlTree(&Processor->WaitTree)) {
            Processor->ClosestWaitTick = UINT64_MAX;
        }
    }

    /* We shouldn't have anything left to do if we haven't expired yet (or if we're the idle
     * thread). */
    if (CurrentThread->ExpirationTicks || CurrentThread == Processor->IdleThread) {
        return;
    }

    /* Now we can raise to SYNCH (block device interrupts) and acquire the processor lock (don't let
     * any other processors mess with us while we mess with the thread queue). */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);

    /* We won't enter idle through here (as we're not forced to), so if there was nothing, just keep
     * on executing the current thread. */
    if (ListHeader == &Processor->ThreadQueue) {
        CurrentThread->ExpirationTicks = PSP_DEFAULT_TICKS;
        KeSetAffinityBit(&KiIdleProcessors, Processor->Number);
        KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
        return;
    } else {
        __atomic_sub_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELEASE);
        __atomic_sub_fetch(&PspGlobalThreadCount, 1, __ATOMIC_RELEASE);
        KeClearAffinityBit(&KiIdleProcessors, Processor->Number);
    }

    PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    PspSwitchThreads(Processor, CurrentThread, TargetThread, PS_STATE_QUEUED, OldIrql);
}
