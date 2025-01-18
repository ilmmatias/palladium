/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
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
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH);
    }

    /* Raise a fatal error if we already acquired this lock on the same thread (recursive/dead
     * lock detected). */
    uint64_t CurrentThread = 1;
    KeProcessor *Processor = HalGetCurrentProcessor();

    if (Processor && Processor->CurrentThread) {
        CurrentThread |= (uint64_t)Processor;
    } else if (Processor) {
        CurrentThread |= (uint64_t)Processor->CurrentThread;
    }

    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) == CurrentThread) {
        KeFatalError(KE_PANIC_SPIN_LOCK_ALREADY_OWNED);
    }

    uint64_t DesiredValue = 0;
    return !__atomic_load_n(Lock, __ATOMIC_RELAXED) &&
           __atomic_compare_exchange_n(
               Lock, &DesiredValue, CurrentThread, 0, __ATOMIC_SEQ_CST, __ATOMIC_ACQUIRE);
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

    /* Raise a fatal error if we already acquired this lock on the same thread (recursive/dead
     * lock detected). */
    uint64_t CurrentThread = 1;
    KeProcessor *Processor = HalGetCurrentProcessor();

    if (Processor && Processor->CurrentThread) {
        CurrentThread |= (uint64_t)Processor;
    } else if (Processor) {
        CurrentThread |= (uint64_t)Processor->CurrentThread;
    }

    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) == CurrentThread) {
        KeFatalError(KE_PANIC_SPIN_LOCK_ALREADY_OWNED);
    }

    while (1) {
        uint64_t DesiredValue = 0;

        if (__atomic_compare_exchange_n(
                Lock, &DesiredValue, CurrentThread, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
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
    if (KeGetIrql() != KE_IRQL_DISPATCH) {
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH);
    }

    /* Raise a fatal error if the lock wasn't acquired by this thread too. */
    uint64_t CurrentThread = 1;
    KeProcessor *Processor = HalGetCurrentProcessor();

    if (Processor && Processor->CurrentThread) {
        CurrentThread |= (uint64_t)Processor;
    } else if (Processor) {
        CurrentThread |= (uint64_t)Processor->CurrentThread;
    }

    if (__atomic_load_n(Lock, __ATOMIC_RELAXED) != CurrentThread) {
        KeFatalError(KE_PANIC_SPIN_LOCK_NOT_OWNED);
    }

    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);
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
        KeFatalError(KE_PANIC_IRQL_NOT_DISPATCH);
    }

    return __atomic_load_n(Lock, __ATOMIC_RELAXED) != 0;
}
