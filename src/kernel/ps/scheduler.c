/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/psp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function switches the execution context between the two given threads.
 *
 * PARAMETERS:
 *     Processor - Current processor block.
 *     CurrentThread - Current executing thread, if any.
 *     CurrentThreadFrame - Current thread frame (if we were executing any thread beforehand).
 *     TargetThread - Target thread to execute.
 *     TargetThreadFrame - Target thread frame.
 *     RequeueThread - Set to 0 if the current thread is entering a waiting or terminated state (and
 *                     shouldn't be requeued).
 *     OldIrql - IRQL before we raised to SYNCH.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void SwitchExecution(
    KeProcessor *Processor,
    PsThread *CurrentThread,
    HalContextFrame *CurrentThreadFrame,
    PsThread *TargetThread,
    HalContextFrame *TargetThreadFrame,
    bool RequeueThread,
    KeIrql OldIrql) {
    /* Set ourselves as busy (no one should try using our context frame until HalpSwitchContext
     * finishes its setup!). */
    if (CurrentThread) {
        __atomic_store_n(&CurrentThreadFrame->Busy, 0x01, __ATOMIC_SEQ_CST);
    }

    /* Requeue the current thread if necessary; We'll just assume CurrentThread points to valid
     * memory if we were asked to requeue it. */
    if (RequeueThread) {
        PspQueueThread(CurrentThread, false);
    }

    /* And switch away (we should be back to lower the IRQL once another thread switches
     * into us). */
    Processor->StackBase = TargetThread->Stack;
    Processor->StackLimit = TargetThread->StackLimit;
    Processor->CurrentThread = TargetThread;
    HalpSwitchContext(CurrentThreadFrame, TargetThreadFrame);
    KeLowerIrql(OldIrql);
}

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
    KeProcessor *Processor = KeGetCurrentProcessor();

    /* The BSP should have queued its initial thread. */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

    PsThread *Thread = Processor->IdleThread;
    if (ListHeader != &Processor->ThreadQueue) {
        Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    }

    /* SwitchContext() should never return (as the current thread frame is NULL). */
    SwitchExecution(Processor, NULL, NULL, Thread, &Thread->ContextFrame, false, OldIrql);
    while (true) {
        StopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function forcefully switches out the current thread.
 *
 * PARAMETERS:
 *     Type - Type of the yield (QUEUE = just yield execution and add to the queue as usual;
 *                               UNQUEUE = wait or terminate state, don't add to the queue).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsYieldThread(int Type) {
    /* Raise to SYNCH (block device interrupts) and acquire the processor lock (to access the
     * queue). */
    KeProcessor *Processor = KeGetCurrentProcessor();
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

    /* If the yield is putting us in a waiting state, we actually want to switch into idle once
     * we have nothing left to do; Otherwise, same as PspProcessQueue (don't switch into idle,
     * stay on the current task). */
    PsThread *TargetThread = Processor->IdleThread;
    if (ListHeader == &Processor->ThreadQueue && Type == PS_YIELD_TYPE_QUEUE) {
        return;
    } else if (ListHeader != &Processor->ThreadQueue) {
        TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
    }

    SwitchExecution(
        Processor,
        Processor->CurrentThread,
        &Processor->CurrentThread->ContextFrame,
        TargetThread,
        &TargetThread->ContextFrame,
        Processor->CurrentThread != Processor->IdleThread && Type == PS_YIELD_TYPE_QUEUE,
        OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles switching the current thread if required. We expect to already be at
 *     the DISPATCH IRQL.
 *
 * PARAMETERS:
 *     InterruptFrame - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspProcessQueue(HalInterruptFrame *) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH, Irql, 0, 0, 0);
    }

    /* We shouldn't have anything to do if the initial thread still isn't running. */
    KeProcessor *Processor = KeGetCurrentProcessor();
    if (!Processor->CurrentThread) {
        return;
    }

    /* Cleanup any threads that have terminated (they shouldn't be the current thread anymore); This
     * needs to be before raising to SYNCH (because other MmFreePool will panic). */
    while (true) {
        RtDList *ListHeader = RtPopDList(&Processor->TerminationQueue);
        if (ListHeader == &Processor->TerminationQueue) {
            break;
        }

        PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        MmFreePool(Thread->Stack, "PsTh");
        MmFreePool(Thread, "PsTh");
    }

    /* Requeue any waiting threads that have expired. */
    for (RtDList *ListHeader = Processor->WaitQueue.Next; ListHeader != &Processor->WaitQueue;) {
        PsThread *Thread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        ListHeader = ListHeader->Next;
        if (!Thread->WaitTicks) {
            RtUnlinkDList(&Thread->ListHeader);
            PspQueueThread(Thread, true);
        }
    }

    /* We shouldn't have anything left to do if we haven't expired yet; Neither if the main thread
     * queue seems empty (in that case, we should get here again once we have sonething to switch
     * into). */
    if (Processor->CurrentThread->ExpirationTicks ||
        Processor->ThreadQueue.Next == &Processor->ThreadQueue) {
        return;
    }

    /* Now we can raise to SYNCH (block device interrupts) and acquire the processor lock (don't let
     * any other processors mess with us while we mess with the thread queue). */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
    RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
    KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

    /* We won't enter idle through here (as we're not forced to), so if there was nothing, just keep
     * on executing the current thread. */
    if (ListHeader == &Processor->ThreadQueue) {
        return;
    }

    PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
    TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
    SwitchExecution(
        Processor,
        Processor->CurrentThread,
        &Processor->CurrentThread->ContextFrame,
        TargetThread,
        &TargetThread->ContextFrame,
        Processor->CurrentThread != Processor->IdleThread,
        OldIrql);
}
