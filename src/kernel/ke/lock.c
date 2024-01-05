/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a kernel spin/busy lock.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeInitializeSpinLock(KeSpinLock *Lock) {
    *Lock = 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gives a single attempt at acquiring a spin lock. We assume the caller is at
 *     DISPATCH level, if not, we crash.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int KeTryAcquireSpinLock(KeSpinLock *Lock) {
    if (KeGetIrql() != KE_IRQL_DISPATCH) {
        KeFatalError(KE_WRONG_IRQL);
    }

    return !__atomic_load_n(Lock, __ATOMIC_RELAXED) &&
           !__atomic_test_and_set(Lock, __ATOMIC_ACQUIRE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the spin lock, waiting if necessary. This will crash if you're at an
 *     IRQL above DISPATCH (DPC); For those cases, install a DPC for the task that needs the
 *     spinlock.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
KeIrql KeAcquireSpinLock(KeSpinLock *Lock) {
    KeIrql Irql = KeRaiseIrql(KE_IRQL_DISPATCH);

    while (1) {
        if (!__atomic_test_and_set(Lock, __ATOMIC_ACQUIRE)) {
            break;
        }

        while (__atomic_load_n(Lock, __ATOMIC_RELAXED)) {
            HalpPauseProcessor();
        }
    }

    return Irql;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases a given spin lock. This will crash if you're at an
 *     IRQL below DISPATCH (DPC); For those cases, install a DPC for the task that needs the
 *     spinlock.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *     NewIrql - At which IRQL KeAcquireSpinLock was called.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeReleaseSpinLock(KeSpinLock *Lock, KeIrql NewIrql) {
    __atomic_clear(Lock, __ATOMIC_RELEASE);
    KeLowerIrql(NewIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a spin lock is currently in use. We assume the caller is at DISPATCH
 *     level, if not, we crash.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     1 if we're locked, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int KeTestSpinLock(KeSpinLock *Lock) {
    if (KeGetIrql() != KE_IRQL_DISPATCH) {
        KeFatalError(KE_WRONG_IRQL);
    }

    return __atomic_load_n(Lock, __ATOMIC_RELAXED);
}
