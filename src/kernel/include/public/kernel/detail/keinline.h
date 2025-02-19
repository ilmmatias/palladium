/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _KERNEL_DETAIL_KEINLINE_H_
#define _KERNEL_DETAIL_KEINLINE_H_

#include <kernel/detail/kefuncs.h>

/* clang-format off */
#if __has_include(ARCH_MAKE_INCLUDE_PATH(kernel/detail, keinline.h))
#include ARCH_MAKE_INCLUDE_PATH(kernel/detail, keinline.h)
#endif /* __has__include */
/* clang-format on */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function raises the current interrupt level; This will panic if a value lower than
 *     the current is passed.
 *
 * PARAMETERS:
 *     NewIrql - Target level.
 *
 * RETURN VALUE:
 *     Old IRQL level.
 *-----------------------------------------------------------------------------------------------*/
static inline KeIrql KeRaiseIrql(KeIrql NewIrql) {
    KeIrql OldIrql = KeGetIrql();
    if (NewIrql < OldIrql) {
        KeFatalError(KE_PANIC_IRQL_NOT_GREATER_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    }

    KeSetIrql(NewIrql);
    return OldIrql;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function lowers the current interrupt level; This will panic if a value higher than
 *     the current is passed.
 *
 * PARAMETERS:
 *     NewIrql - Target level.
 *
 * RETURN VALUE:
 *     Old IRQL level.
 *-----------------------------------------------------------------------------------------------*/
static inline void KeLowerIrql(KeIrql NewIrql) {
    KeIrql OldIrql = KeGetIrql();
    if (OldIrql < NewIrql) {
        KeFatalError(KE_PANIC_IRQL_NOT_LESS_OR_EQUAL, OldIrql, NewIrql, 0, 0);
    }

    KeSetIrql(NewIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gives a single attempt at acquiring a spin lock. We do not check or try to
 *     raise the current IRQL.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     true on success, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static inline bool KeTryAcquireSpinLockAtCurrentIrql(KeSpinLock *Lock) {
    return !__atomic_load_n(Lock, __ATOMIC_RELAXED) &&
           !(__atomic_fetch_or(Lock, 0x01, __ATOMIC_ACQUIRE) & 0x01);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the spin lock, waiting if necessary. We do not check or try to
 *     raise the current IRQL.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void KeAcquireSpinLockAtCurrentIrql(KeSpinLock *Lock) {
    while (true) {
        if (!(__atomic_fetch_or(Lock, 0x01, __ATOMIC_ACQUIRE) & 0x01)) {
            break;
        }

        while (__atomic_load_n(Lock, __ATOMIC_RELAXED)) {
            PauseProcessor();
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function raises the IRQL, and acquires the spin lock, waiting if necessary.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *     NewIrql - Target we should raise the IRQL to.
 *
 * RETURN VALUE:
 *     Previous IRQL value.
 *-----------------------------------------------------------------------------------------------*/
static inline KeIrql KeAcquireSpinLockAndRaiseIrql(KeSpinLock *Lock, KeIrql NewIrql) {
    KeIrql OldIrql = KeRaiseIrql(NewIrql);
    KeAcquireSpinLockAtCurrentIrql(Lock);
    return OldIrql;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases a given spin lock. We do not check or try to lower the current IRQL.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void KeReleaseSpinLockAtCurrentIrql(KeSpinLock *Lock) {
    __atomic_store_n(Lock, 0, __ATOMIC_RELEASE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases a given spin lock and lowers the IRQL.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *     NewIrql - At which IRQL KeAcquireSpinLock was called.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void KeReleaseSpinLockAndLowerIrql(KeSpinLock *Lock, KeIrql NewIrql) {
    KeReleaseSpinLockAtCurrentIrql(Lock);
    KeLowerIrql(NewIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if a spin lock is currently in use. We do not check the current IRQL.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     true if we're locked, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
static inline bool KeTestSpinLockAtCurrentIrql(KeSpinLock *Lock) {
    return __atomic_load_n(Lock, __ATOMIC_RELAXED) != 0;
}

#endif /* _KERNEL_DETAIL_KEINLINE_H_ */
