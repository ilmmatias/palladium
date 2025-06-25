/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/evp.h>
#include <kernel/ke.h>
#include <kernel/obp.h>
#include <kernel/ps.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to acquire the given mutex, and updates its contention according to
 *     the caller's intention. This is the internal implementation of TryAcquireMutex, and should be
 *     called with the lock already owned.
 *
 * PARAMETERS:
 *     Mutex - Which mutex object to try acquiring.
 *     IncreaseContention - Set this to true if we should increase the contention by 1.
 *
 * RETURN VALUE:
 *     Either true (we acquired the mutex), or false (we didn't).
 *-----------------------------------------------------------------------------------------------*/
static bool TryAcquire(EvMutex *Mutex, bool IncreaseContention) {
    PsThread *Thread = PsGetCurrentThread();

    /* Owner is increasing the recursion; Let's just hope the caller remembers to ReleaseMutex() as
     * many times as they acquired it...  */
    if (Mutex->Owner == Thread) {
        Mutex->Recursion++;
        return true;
    }

    /* Trying to acquire the lock with no contention, we're done if the lock has no owner in this
     * case. */
    if (!Mutex->Contention && !Mutex->Owner) {
        Mutex->Header.Signaled = false;
        Mutex->Recursion = 1;
        Mutex->Owner = Thread;
        return true;
    }

    /* Otherwise, update the contention according to the caller's intent. */
    if (IncreaseContention) {
        Mutex->Contention++;
    }

    return false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates a new binary mutex.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Either a pointer to the mutex object, or NULL on allocation failure.
 *-----------------------------------------------------------------------------------------------*/
EvMutex *EvCreateMutex(void) {
    EvMutex *Mutex = ObCreateObject(&ObpMutexType, MM_POOL_TAG_EVENT);

    if (Mutex) {
        Mutex->Header.Type = EV_TYPE_MUTEX;
        Mutex->Header.Signaled = true;
        RtInitializeDList(&Mutex->Header.WaitList);
    }

    return Mutex;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries acquiring the mutex, increasing the recursion count if the current thread
 *     already owns the mutex.
 *
 * PARAMETERS:
 *     Mutex - Which mutex object to try acquiring.
 *
 * RETURN VALUE:
 *     true if we acquired the object, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool EvTryAcquireMutex(EvMutex *Mutex) {
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Mutex->Header.Lock, KE_IRQL_DISPATCH);
    bool Result = TryAcquire(Mutex, false);
    KeReleaseSpinLockAndLowerIrql(&Mutex->Header.Lock, OldIrql);
    return Result;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loops until we are able to acquire the mutex, increasing the recursion count if
 *     the current thread already owns the mutex. This function will block if the mutex cannot be
 *     currently acquired (until the mutex is acquired, or the timeout is reached).
 *
 * PARAMETERS:
 *     Mutex - Which mutex object to acquire.
 *     Timeout - Maximum amount to wait until the mutex is acquired.
 *
 * RETURN VALUE:
 *     true if the lock was acquired before the timeout, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool EvAcquireMutex(EvMutex *Mutex, uint64_t Timeout) {
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Mutex->Header.Lock, KE_IRQL_DISPATCH);

    /* Check if we can take the fast path (or update the contention and get ready to wait). */
    if (TryAcquire(Mutex, true)) {
        KeReleaseSpinLockAndLowerIrql(&Mutex->Header.Lock, OldIrql);
        return true;
    }

    KeReleaseSpinLockAndLowerIrql(&Mutex->Header.Lock, OldIrql);

    /* Let's use WaitForObject and rely on EvReleaseMutex only waking up one thread at a time
     * (so when it returns, it's either timeout or we're safe to setup the lock). */
    if (!EvWaitForObject(Mutex, Timeout)) {
        OldIrql = KeAcquireSpinLockAndRaiseIrql(&Mutex->Header.Lock, KE_IRQL_DISPATCH);
        Mutex->Contention--;
        KeReleaseSpinLockAndLowerIrql(&Mutex->Header.Lock, OldIrql);
        return false;
    }

    Mutex->Recursion = 1;
    Mutex->Owner = PsGetCurrentThread();
    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function decreases the recursion count for the given mutex, releasing it when the count
 *     reaches zero. Only the owner of the lock should call this function, failure to follow that
 *     will result in an exception/panic.
 *
 * PARAMETERS:
 *     Mutex - Which mutex object to try acquiring.
 *
 * RETURN VALUE:
 *     true if we acquired the object, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
void EvReleaseMutex(EvMutex *Mutex) {
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Mutex->Header.Lock, KE_IRQL_DISPATCH);

    /* Once we add user mode, we should probably try raising an exception first (and hopefully
     * finish just the user process), but for now, crash if we don't own this. */
    PsThread *Thread = PsGetCurrentThread();
    if (Mutex->Owner != Thread) {
        KeFatalError(
            KE_PANIC_MUTEX_NOT_OWNED,
            (uint64_t)Thread,
            (uint64_t)Mutex->Owner,
            Mutex->Recursion,
            Mutex->Contention);
    } else if (!--Mutex->Recursion) {
        /* Take a bit of caution here; If we just set ourselves as signaled and wake up the next
         * thread, that thread and someone else that just called WaitForObject might see it as
         * signaled/acquirable, and that would cause a lot of trouble. */
        Mutex->Owner = NULL;
        if (Mutex->Contention) {
            Mutex->Contention--;
            EvpWakeSingleThread(&Mutex->Header);
        } else {
            Mutex->Header.Signaled = true;
        }
    }

    KeReleaseSpinLockAndLowerIrql(&Mutex->Header.Lock, OldIrql);
}
