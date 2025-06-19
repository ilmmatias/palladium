/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/rand.h>
#include <kernel/halp.h>
#include <kernel/mi.h>
#include <kernel/psp.h>
#include <kernel/vid.h>

extern KeAffinity KiIdleProcessors;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to choose and steal a victim thread from another processor. We should
 *     only be called if no other threads are available for us to execute.
 *
 * PARAMETERS:
 *     Processor - Pointer to the current processor structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static PsThread *TrySteal(KeProcessor *Processor) {
    /* Start the search at a random index so that there's less chance multiple idle processors will
     * compete for the same lock. */
    uint64_t StartIndex = __rand64() % HalpOnlineProcessorCount;
    uint64_t CurrentIndex = StartIndex;

    do {
        KeProcessor *TargetProcessor = HalpProcessorList[CurrentIndex];
        if (TargetProcessor == Processor) {
            CurrentIndex = (CurrentIndex + 1) % HalpOnlineProcessorCount;
            continue;
        }

        /* Don't bother if there doesn't seem to be any threads we can steal. */
        if (__atomic_load_n(&TargetProcessor->ThreadCount, __ATOMIC_RELAXED) < 2) {
            CurrentIndex = (CurrentIndex + 1) % HalpOnlineProcessorCount;
            continue;
        }

        /* Try locking, but don't spin, just move onwards if we can't acquire it. */
        KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);
        if (!KeTryAcquireSpinLockAtCurrentIrql(&TargetProcessor->Lock)) {
            KeLowerIrql(OldIrql);
            CurrentIndex = (CurrentIndex + 1) % HalpOnlineProcessorCount;
            continue;
        }

        /* Attempt to grab the victim thread; After this, we can then unlock (and lower the IRQL),
         * and return early if we already have something. */
        RtDList *TargetThread = RtTruncateDList(&TargetProcessor->ThreadQueue);
        KeReleaseSpinLockAndLowerIrql(&TargetProcessor->Lock, OldIrql);
        if (TargetThread != &TargetProcessor->ThreadQueue) {
            __atomic_sub_fetch(&TargetProcessor->ThreadCount, 1, __ATOMIC_RELAXED);
            return CONTAINING_RECORD(TargetThread, PsThread, ListHeader);
        }

        CurrentIndex = (CurrentIndex + 1) % HalpOnlineProcessorCount;
    } while (CurrentIndex != StartIndex);

    return NULL;
}

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
        /* Try to execute some general system cleanup. */
        MiTryReturnKernelStacks();

        /* Let the processor rest for a bit before continuing. */
        PauseProcessor();

        /* If required, try and steal something from another processor. */
        PsThread *TargetThread = NULL;
        if (Processor->ThreadQueue.Next == &Processor->ThreadQueue) {
            TargetThread = TrySteal(Processor);
        }

        /* Do we have any threads available to swap into? If not, then loop back (pause and retry).
         */
        if (!TargetThread && Processor->ThreadQueue.Next == &Processor->ThreadQueue) {
            continue;
        }

        /* If we do, block preemption and get ready for a swap. */
        KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Processor->Lock, KE_IRQL_SYNCH);

        if (!TargetThread) {
            RtDList *ListHeader = RtPopDList(&Processor->ThreadQueue);
            if (ListHeader == &Processor->ThreadQueue) {
                /* Between the check and actually accesing the queue, someone stole our thread;
                 * We're idle so this really shouldn't have happened, but whatever, just unlock and
                 * keep on spinning. */
                KeReleaseSpinLockAndLowerIrql(&Processor->Lock, OldIrql);
                VidPrint(
                    VID_MESSAGE_DEBUG,
                    "Kernel Scheduler",
                    "processor %u got its new thread stolen while idle\n",
                    Processor->Number);
                continue;
            } else {
                __atomic_sub_fetch(&Processor->ThreadCount, 1, __ATOMIC_RELAXED);
                TargetThread = CONTAINING_RECORD(ListHeader, PsThread, ListHeader);
            }
        }

        KeClearAffinityBit(&KiIdleProcessors, Processor->Number);
        PspSwitchThreads(Processor, Processor->IdleThread, TargetThread, PS_STATE_IDLE, OldIrql);
    }
}
