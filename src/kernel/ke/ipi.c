/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>

static KeSpinLock Lock = {0};
static void (*TargetRoutine)(void *) = NULL;
static void *TargetParameter = NULL;
static volatile uint64_t EarlyBarrier = 0;
static volatile uint64_t LateBarrier = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function waits until all processors have reached a common execution standpoint.
 *
 * PARAMETERS:
 *     State - Counter to be used for the synchronization; Should only be initialized by one
 *             processor.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeSynchronizeProcessors(volatile uint64_t *State) {
    __atomic_fetch_add(State, 1, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(State, __ATOMIC_RELAXED) != HalpOnlineProcessorCount) {
        PauseProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function requests all processors to run a routine at KE_IRQL_IPI.
 *
 * PARAMETERS:
 *     Routine - Which routine to run.
 *     Parameter - Parameter to the routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeRequestIpiRoutine(void (*Routine)(void *), void *Parameter) {
    /* Lock ourselves out of being interrupted by other IPIs. */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Lock, KE_IRQL_IPI);

    /* Initialize the synchronization state. */
    TargetRoutine = Routine;
    TargetParameter = Parameter;
    __atomic_store_n(&EarlyBarrier, 0, __ATOMIC_SEQ_CST);
    __atomic_store_n(&LateBarrier, 0, __ATOMIC_SEQ_CST);

    /* Interrupt all processors into the IPI handler. */
    HalpBroadcastIpi();

    /* Synchronize all processors before running anything. */
    KeSynchronizeProcessors(&EarlyBarrier);
    Routine(Parameter);

    /* And synchronize afterwards as well. */
    KeSynchronizeProcessors(&LateBarrier);
    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles an incoming IPI request.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiHandleIpi(void) {
    /* No need for a full SynchronizeProcessors after running the target routine, we're just
     * supposed to notify the starting processor that we finished. */
    KeSynchronizeProcessors(&EarlyBarrier);
    TargetRoutine(TargetParameter);
    __atomic_fetch_add(&LateBarrier, 1, __ATOMIC_SEQ_CST);
}
