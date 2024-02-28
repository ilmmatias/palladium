/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _MM_HXX_
#define _MM_HXX_

#include <mm.h>

template <typename Type>
class AutoPtr {
   public:
    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new pointer with automatic reference counting.
     *     This should prevent leaks (as long as you don't create any circular references), so it is
     *     the recommended way to do things in C++!
     *     For pointers that need no sharing (no reference counting, should always be dropped at
     *     the end of the scope), use ScopePtr<Type> instead!
     *
     * PARAMETERS:
     *     Tag - Which tag should be associated with this allocation.
     *
     * RETURN VALUE:
     *     Reference counted pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    AutoPtr(const char Tag[4]) {
        m_data = (Type *)MmAllocatePool(sizeof(Type), Tag);
        if (!m_data) {
            return;
        }

        m_references = (int *)MmAllocatePool(sizeof(int), "Arc ");
        if (!m_references) {
            MmFreePool(m_data, Tag);
        }

        *m_references = 1;
        memcpy(m_tag, Tag, 4);
    }

    AutoPtr(AutoPtr &Other) {
        m_data = Other.m_data;
        m_references = Other.m_references;
        __atomic_add_fetch(m_references, 1, __ATOMIC_SEQ_CST);
        memcpy(m_tag, Other.m_tag, 4);
    }

    ~AutoPtr(void) {
        /* Checking for exact zero should prevent multiple threads accidentally freeing the
         * data at the same time. */
        if (m_references && __atomic_sub_fetch(m_references, 1, __ATOMIC_SEQ_CST) == 0) {
            MmFreePool(m_references, "Arc ");
            MmFreePool(m_data, m_tag);
        }
    }

    operator bool(void) const {
        return m_data != NULL;
    }

    template <typename PointerType>
    operator PointerType *(void) const {
        return (PointerType *)m_data;
    }

    bool operator!() const {
        return m_data == NULL;
    }

    Type &operator*(void) const {
        return *m_data;
    }

    Type *operator->(void) const {
        return m_data;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function returns a raw pointer to the allocated data (or NULL if we failed/were
     *     reset); The pointer cannot be used after the reference counter reaches zero!
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     Raw pointer to the data.
     *-----------------------------------------------------------------------------------------------*/
    Type *Data(void) const {
        return m_data;
    }

   private:
    Type *m_data;
    char m_tag[4];
    int *m_references;
};

template <typename Type>
class ScopePtr {
   public:
    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new pointer with an auto delete at the end of the scope.
     *     For pointers that need sharing (with reference counting), use AutoPtr<Type> instead!
     *
     * PARAMETERS:
     *     Tag - Which tag should be associated with this allocation.
     *
     * RETURN VALUE:
     *     Wrapped pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    ScopePtr(const char Tag[4]) {
        m_data = (Type *)MmAllocatePool(sizeof(Type), Tag);
        memcpy(m_tag, Tag, 4);
    }

    /* Only valid in this scope, we can't let anyone copy the pointer! */
    ScopePtr(ScopePtr &Other) = delete;

    ~ScopePtr(void) {
        MmFreePool(m_data, m_tag);
    }

    operator bool(void) const {
        return m_data != NULL;
    }

    template <typename PointerType>
    operator PointerType *(void) const {
        return (PointerType *)m_data;
    }

    bool operator!() const {
        return m_data == NULL;
    }

    Type &operator*(void) const {
        return *m_data;
    }

    Type *operator->(void) const {
        return m_data;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function returns a raw pointer to the allocated data (or NULL if we failed/were
     *     reset); The pointer cannot be used after the end of the scope!
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     Raw pointer to the data.
     *-----------------------------------------------------------------------------------------------*/
    Type *Data(void) const {
        return m_data;
    }

   private:
    Type *m_data;
    char m_tag[4];
};

#endif /* _MM_HXX_ */
