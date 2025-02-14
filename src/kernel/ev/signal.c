/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/evp.h>
#include <kernel/ke.h>
#include <kernel/mm.h>
#include <kernel/obp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a new simple signal event (that can be notified via Set/ClearSignal).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Either a pointer to the signal object, or NULL on allocation failure.
 *-----------------------------------------------------------------------------------------------*/
EvSignal *EvCreateSignal(void) {
    EvSignal *Signal = ObCreateObject(&ObpSignalType, MM_POOL_TAG_EVENT);

    if (Signal) {
        Signal->Header.Type = EV_TYPE_SIGNAL;
        RtInitializeDList(&Signal->Header.WaitList);
    }

    return Signal;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the signal state, and notifies all waiting threads.
 *
 * PARAMETERS:
 *     Signal - Which signal object to set.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvSetSignal(EvSignal *Signal) {
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Signal->Header.Lock, KE_IRQL_DISPATCH);
    Signal->Header.Signaled = true;
    EvpWakeAllThreads(&Signal->Header);
    KeReleaseSpinLockAndLowerIrql(&Signal->Header.Lock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets/clears the signal state.
 *
 * PARAMETERS:
 *     Signal - Which signal object to clear.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvClearSignal(EvSignal *Signal) {
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Signal->Header.Lock, KE_IRQL_DISPATCH);
    Signal->Header.Signaled = false;
    KeReleaseSpinLockAndLowerIrql(&Signal->Header.Lock, OldIrql);
}
