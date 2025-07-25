/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/mm.h>
#include <kernel/ps.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given thread-level asynchronous alert object; The
 *     initialization is done in a generic way, and the alert object can later be enqueued by any
 *     processor into any thread.
 *
 * PARAMETERS:
 *     Alert - Pointer to the alert object structure.
 *     Routine - Which function should be executed when the alert is called.
 *     Context - Some opaque data to be passed into the alert routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsInitializeAlert(PsAlert* Alert, void (*Routine)(void*), void* Context) {
    Alert->Routine = Routine;
    Alert->Context = Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function enqueues the given alert object to be executed in the target thread whenever
 *     possible.
 *
 * PARAMETERS:
 *     Thread - Which thread the alert should run under.
 *     Alert - Pointer to the initialized alert object structure.
 *
 * RETURN VALUE:
 *     false if we failed to insert the alert (the thread probably got terminated), false
 *     otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool PsQueueAlert(PsThread* Thread, PsAlert* Alert) {
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Thread->AlertLock, KE_IRQL_DISPATCH);
    if (__atomic_load_n(&Thread->AlertListBlocked, __ATOMIC_RELAXED)) {
        KeReleaseSpinLockAndLowerIrql(&Thread->AlertLock, OldIrql);
        return false;
    }

    RtPushSList(&Thread->AlertList, &Alert->ListHeader);
    KeReleaseSpinLockAndLowerIrql(&Thread->AlertLock, OldIrql);
    if (Thread->State == PS_STATE_RUNNING) {
        HalpNotifyProcessor(Thread->Processor, KE_IRQL_ALERT);
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function empties the alert queues for the current thread.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PspProcessAlertQueue(void) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_ALERT) {
        KeFatalError(KE_PANIC_IRQL_NOT_EQUAL, KE_IRQL_ALERT, Irql, 0, 0);
    }

    PsThread* Thread = PsGetCurrentThread();
    while (true) {
        KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Thread->AlertLock, KE_IRQL_DISPATCH);
        RtSList* ListHeader = RtPopSList(&Thread->AlertList);
        KeReleaseSpinLockAndLowerIrql(&Thread->AlertLock, OldIrql);
        if (ListHeader) {
            /* We'll be assuming any alert data is dynamically allocated (TAG_NONE cleans up any
             * data, no matter the tag). */
            PsAlert* Alert = CONTAINING_RECORD(ListHeader, PsAlert, ListHeader);
            Alert->Routine(Alert->Context);
            MmFreePool(Alert, MM_POOL_TAG_NONE);
        }
    }
}
