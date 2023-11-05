/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>

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
 *     This function gives a single attempt at acquiring a spin lock.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     1 on success, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int KeTryAcquireSpinLock(KeSpinLock *Lock) {
    return !__atomic_load_n(Lock, __ATOMIC_RELAXED) &&
           !__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the spin lock, waiting if necessary.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeAcquireSpinLock(KeSpinLock *Lock) {
    while (1) {
        if (!__atomic_exchange_n(Lock, 1, __ATOMIC_ACQUIRE)) {
            break;
        }

        while (__atomic_load_n(Lock, __ATOMIC_RELAXED))
            ;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases a given spin lock.
 *
 * PARAMETERS:
 *     Lock - Struct containing the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeReleaseSpinLock(KeSpinLock *Lock) {
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
    return __atomic_load_n(Lock, __ATOMIC_RELAXED);
}
