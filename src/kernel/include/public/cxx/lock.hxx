/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_LOCK_HXX_
#define _CXX_LOCK_HXX_

#include <ke.h>

class SpinLockGuard {
   public:
    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a scope-managed spin lock guard.
     *     Use this instead of KeAcquire/ReleaseSpinLock, unless you cannot use C++ in the current
     *     context.
     *
     * PARAMETERS:
     *     Lock - Which spin lock should be locked (and managed).
     *
     * RETURN VALUE:
     *     Scope-managed guard for the spin lock.
     *-----------------------------------------------------------------------------------------------*/
    SpinLockGuard(KeSpinLock *Lock) {
        m_lock = Lock;
        m_irql = KeAcquireSpinLock(Lock);
    }

    ~SpinLockGuard(void) {
        if (m_lock) {
            KeReleaseSpinLock(m_lock, m_irql);
        }
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function does an early release of the held lock; This will make the deconstructor be
     *     a noop.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Release(void) {
        KeReleaseSpinLock(m_lock, m_irql);
        m_lock = NULL;
    }

   private:
    KeSpinLock *m_lock;
    KeIrql m_irql;
};

#endif /* _CXX_LOCK_HXX_ */
