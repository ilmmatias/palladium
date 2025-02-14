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

        PsThread *TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
        PspSwitchThreads(Processor, Processor->IdleThread, TargetThread, PS_STATE_IDLE, OldIrql);
    }
}
