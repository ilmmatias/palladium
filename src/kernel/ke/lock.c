/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This (inline) function calculates the target lock value (for detecting deadlocks).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Target lock value.
 *-----------------------------------------------------------------------------------------------*/
static inline uint64_t GetTargetLockValue(void) {
    KeProcessor *Processor = HalGetCurrentProcessor();
    if (Processor && Processor->CurrentThread) {
        return (uint64_t)Processor->CurrentThread | 1;
    } else if (Processor) {
        return (uint64_t)Processor | 1;
    } else {
        return 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This (inline) function loops until we acquire the spinlock.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *     TargetValue - Value to be stored on the lock once we obtain it.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void AcquireLock(KeSpinLock *Lock, uint64_t TargetValue) {
    while (1) {
        uint64_t DesiredValue = 0;

        if (__atomic_compare_exchange_n(
                Lock, &DesiredValue, TargetValue, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
            break;
        }

        while (__atomic_load_n(Lock, __ATOMIC_RELAXED)) {
            HalpPauseProcessor();
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gives a single attempt at acquiring a spin lock. We assume the caller is at
 *     DISPATCH level or above, if not, we crash.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int KeTryAcquireSpinLockHighIrql(KeSpinLock *Lock) {
    KeIrql Irql = KeGetIrql();
    if (Irql < KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_GREATER_OR_EQUAL, Irql, KE_IRQL_DISPATCH, 0, 0);
    }

    /* Raise a fatal error if we already acquired this lock on the same thread (recursive/dead
     * lock detected). */
    uint64_t TargetValue = GetTargetLockValue();
    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) == TargetValue) {
        KeFatalError(KE_PANIC_SPIN_LOCK_ALREADY_OWNED, (uint64_t)Lock, TargetValue, 0, 0);
    }

    uint64_t DesiredValue = 0;
    return !__atomic_load_n(Lock, __ATOMIC_RELAXED) &&
           __atomic_compare_exchange_n(
               Lock, &DesiredValue, TargetValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function raises the IRQL to DISPATCH, and acquires the spin lock, waiting if necessary.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     Previous IRQL value.
 *-----------------------------------------------------------------------------------------------*/
KeIrql KeAcquireSpinLock(KeSpinLock *Lock) {
    KeIrql Irql = KeRaiseIrql(KE_IRQL_DISPATCH);

    /* Raise a fatal error if we already acquired this lock on the same thread (recursive/dead
     * lock detected). */
    uint64_t TargetValue = GetTargetLockValue();
    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) == TargetValue) {
        KeFatalError(KE_PANIC_SPIN_LOCK_ALREADY_OWNED, (uint64_t)Lock, TargetValue, 0, 0);
    }

    AcquireLock(Lock, TargetValue);
    return Irql;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the spin lock, waiting if necessary. Unlike KeAcquireSpinLock(), we
 *     don't try to raise the IRQL, so we can be used on IRQL>DISPATCH as well.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeAcquireSpinLockHighIrql(KeSpinLock *Lock) {
    /* Raise a fatal error if we already acquired this lock on the same thread (recursive/dead
     * lock detected). */
    uint64_t TargetValue = GetTargetLockValue();
    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) == TargetValue) {
        KeFatalError(KE_PANIC_SPIN_LOCK_ALREADY_OWNED, (uint64_t)Lock, TargetValue, 0, 0);
    }

    AcquireLock(Lock, TargetValue);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases a given spin lock. We assume the caller is at DISPATCH
 *     level, if not, we crash.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *     NewIrql - At which IRQL KeAcquireSpinLock was called.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeReleaseSpinLock(KeSpinLock *Lock, KeIrql NewIrql) {
    KeIrql Irql = KeGetIrql();
    if (Irql != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH, Irql, 0, 0, 0);
    }

    /* Raise a fatal error if the lock wasn't acquired by this thread too. */
    uint64_t TargetValue = GetTargetLockValue();
    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) != TargetValue) {
        KeFatalError(KE_PANIC_SPIN_LOCK_NOT_OWNED, (uint64_t)Lock, TargetValue, 0, 0);
    }

    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);
    KeLowerIrql(NewIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases a given spin lock. This function should be used together with
 *     KeAcquireSpinLockHighIrql, as neither raises/lowers the IRQL.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeReleaseSpinLockHighIrql(KeSpinLock *Lock) {
    /* Raise a fatal error if the lock wasn't acquired by this thread too. */
    uint64_t TargetValue = GetTargetLockValue();
    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) != TargetValue) {
        KeFatalError(KE_PANIC_SPIN_LOCK_NOT_OWNED, (uint64_t)Lock, TargetValue, 0, 0);
    }

    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a spin lock is currently in use.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     1 if we're locked, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int KeTestSpinLock(KeSpinLock *Lock) {
    return __atomic_load_n(Lock, __ATOMIC_RELAXED) != 0;
}
