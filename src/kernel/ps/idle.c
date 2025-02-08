/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>
#include <kernel/psp.h>
#include <kernel/vid.h>

extern KeAffinity KiIdleProcessors;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes when a processor has no threads to execute.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void PspIdleThread(void *) {
    /* This value should always be the same (because the idle thread can't be moved between
     * processors). */
    KeProcessor *Processor = KeGetCurrentProcessor();

    while (true) {
        /* Let the processor rest for a bit before continuing. */
        PauseProcessor();

        /* Do we have any threads available to swap into? */
        if (Processor->ThreadQueue.Next == &Processor->ThreadQueue) {
            continue;
        }

        /* If so, block preemption and get ready for a swap. */
        KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);
        RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);

        if (ListHeader == &Processor->ThreadQueue) {
            /* Between the check and actually accesing the queue, someone stole our thread; We're
             * idle so this really shouldn't have happened, but whatever, just unlock and keep on
             * spinning. */
            KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
            VidPrint(
                VID_MESSAGE_DEBUG,
                "Kernel Scheduler",
                "processor %u got its new thread stolen while idle\n",
                Processor->Number);
            continue;
        } else {
            KeClearAffinityBit(&KiIdleProcessors, Processor->Number);
        }

        /* Mark the newly chosen target as the current one. */
        PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        TargetThread->State = PS_STATE_RUNNING;
        TargetThread->ExpirationTicks = PSP_DEFAULT_TICKS;
        Processor->CurrentThread = TargetThread;
        Processor->StackBase = TargetThread->Stack;
        Processor->StackLimit = TargetThread->StackLimit;

        /* Mark ourselves as doing a context switch; No need to change the state (it's always idle)
         * nor requeue. */
        __atomic_store_n(&Processor->IdleThread->ContextFrame.Busy, 0x01, __ATOMIC_SEQ_CST);
        KeReleaseSpinLockAtCurrentIrql(&Processor->Lock);

        /* Swap into the new thread; We should be back to lower the IRQL the processor gets idle
         * again. */
        HalpSwitchContext(&Processor->IdleThread->ContextFrame, &TargetThread->ContextFrame);
        KeLowerIrql(OldIrql);
    }
}
