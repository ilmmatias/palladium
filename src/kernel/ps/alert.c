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
 *     Flags - Some flags specifiying behaviour for the alert object; You more than likely want to
 *             just use PS_INIT_ALERT_DEFAULT.
 *     Routine - Which function should be executed when the alert is called.
 *     Context - Some opaque data to be passed into the alert routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PsInitializeAlert(PsAlert* Alert, uint64_t Flags, void (*Routine)(void*), void* Context) {
    Alert->Routine = Routine;
    Alert->Context = Context;
    Alert->Queued = false;
    Alert->PoolAllocated = (Flags & PS_INIT_ALERT_POOL_ALLOCATED) ? true : false;
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
 *     false if we failed to insert the alert (the thread got terminated, or someone else already
 *     queued this alert), false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool PsQueueAlert(PsThread* Thread, PsAlert* Alert) {
    bool ExpectedValue = false;
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Thread->AlertLock, KE_IRQL_DISPATCH);
    if (__atomic_load_n(&Thread->AlertListBlocked, __ATOMIC_RELAXED)) {
        KeReleaseSpinLockAndLowerIrql(&Thread->AlertLock, OldIrql);
        return false;
    } else if (!__atomic_compare_exchange_n(
                   &Alert->Queued, &ExpectedValue, true, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE)) {
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
        if (!ListHeader) {
            continue;
        }

        PsAlert* Alert = CONTAINING_RECORD(ListHeader, PsAlert, ListHeader);
        __atomic_store_n(&Alert->Queued, false, __ATOMIC_RELEASE);
        Alert->Routine(Alert->Context);
    }
}
