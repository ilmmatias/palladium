/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_CRITICAL_SECTION_HXX_
#define _CXX_CRITICAL_SECTION_HXX_

#include <halp.h>

class CriticalSection {
   public:
    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a scope-managed critical section.
     *     Use this instead of HalpEnter/LeaveCriticalSection, unless you cannot use C++ in the
     *     current context.
     *
     * PARAMETERS:
     *     None
     *
     * RETURN VALUE:
     *     Scope-managed critical guard.
     *-----------------------------------------------------------------------------------------------*/
    CriticalSection(void) {
        m_context = HalpEnterCriticalSection();
    }

    ~CriticalSection(void) {
        if (m_context) {
            HalpLeaveCriticalSection(m_context);
        }
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function does an early exit out of the critical section; This will make the
     *     deconstructor be a noop.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Leave(void) {
        HalpLeaveCriticalSection(m_context);
        m_context = NULL;
    }

   private:
    void *m_context;
};

#endif /* _CXX_CRITICAL_SECTION_HXX_ */
