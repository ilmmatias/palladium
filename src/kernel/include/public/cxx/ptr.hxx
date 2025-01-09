/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_PTR_HXX_
#define _CXX_PTR_HXX_

#include <mm.h>
#include <string.h>

#include <cxx/new.hxx>

template <typename Type>
class AutoPtr {
   public:
    AutoPtr() {
    }

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
     *     ArgList - Arguments to be forwarded into the type constructor.
     *
     * RETURN VALUE:
     *     Reference counted pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    template <typename... Args>
    AutoPtr(const char Tag[4], Args &&...ArgList) {
        void *Data = MmAllocatePool(sizeof(Type), Tag);
        if (!Data) {
            return;
        }

        m_references = (int *)MmAllocatePool(sizeof(int), "Arc ");
        if (!m_references) {
            MmFreePool(Data, Tag);
            return;
        }

        m_data = new (Data) Type(ArgList...);
        *m_references = 1;
        memcpy(m_tag, Tag, 4);
    }

    AutoPtr(AutoPtr &&Other) {
        m_data = Other.m_data;
        m_references = Other.m_references;
        Other.m_data = NULL;
        Other.m_references = NULL;
        memcpy(m_tag, Other.m_tag, 4);
    }

    AutoPtr(const AutoPtr &Other) {
        m_data = Other.m_data;
        m_references = Other.m_references;
        __atomic_add_fetch(m_references, 1, __ATOMIC_SEQ_CST);
        memcpy(m_tag, Other.m_tag, 4);
    }

    ~AutoPtr(void) {
        /* Checking for exact zero should prevent multiple threads accidentally freeing the
         * data at the same time. */
        if (m_references && __atomic_sub_fetch(m_references, 1, __ATOMIC_SEQ_CST) == 0) {
            m_data->~Type();
            MmFreePool(m_references, "Arc ");
            MmFreePool(m_data, m_tag);
        }
    }

    AutoPtr &operator=(const AutoPtr &Other) {
        if (this != &Other) {
            this->~AutoPtr();
            m_data = Other.m_data;
            m_references = Other.m_references;
            __atomic_add_fetch(m_references, 1, __ATOMIC_SEQ_CST);
            memcpy(m_tag, Other.m_tag, 4);
        }

        return *this;
    }

    AutoPtr &operator=(AutoPtr &&Other) {
        if (this != &Other) {
            this->~AutoPtr();
            m_data = Other.m_data;
            m_references = Other.m_references;
            Other.m_data = NULL;
            Other.m_references = NULL;
            memcpy(m_tag, Other.m_tag, 4);
        }

        return *this;
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
     *     This function creates a move reference, ready for our move constructor.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     Move reference to the pointer.
     *-----------------------------------------------------------------------------------------------*/
    AutoPtr &&Move(void) {
        return (AutoPtr &&)*this;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function resets the current pointer, decreasing the RC before the scope's end.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Reset(void) {
        this->~AutoPtr();
        m_data = NULL;
        m_references = NULL;
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
    int *m_references;
    char m_tag[4];
};

template <typename Type>
class ScopePtr {
   public:
    ScopePtr() {
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new pointer with an auto delete at the end of the scope.
     *     For pointers that need sharing (with reference counting), use AutoPtr<Type> instead!
     *
     * PARAMETERS:
     *     Tag - Which tag should be associated with this allocation.
     *     ArgList - Arguments to be forwarded into the type constructor.
     *
     * RETURN VALUE:
     *     Wrapped pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    template <typename... Args>
    ScopePtr(const char Tag[4], Args &&...ArgList) {
        void *Data = MmAllocatePool(sizeof(Type), Tag);
        if (Data) {
            m_data = new (Data) Type(ArgList...);
            memcpy(m_tag, Tag, 4);
        }
    }

    ScopePtr(ScopePtr &&Other) {
        m_data = Other.m_data;
        Other.m_data = NULL;
        memcpy(m_tag, Other.m_tag, 4);
    }

    /* Only valid in this scope, we can't let anyone copy the pointer! */
    ScopePtr(const ScopePtr &Other) = delete;
    ScopePtr &operator=(const ScopePtr &Other) = delete;

    ~ScopePtr(void) {
        if (m_data) {
            m_data->~Type();
            MmFreePool(m_data, m_tag);
        }
    }

    ScopePtr &operator=(ScopePtr &&Other) {
        if (this != &Other) {
            this->~ScopePtr();
            m_data = Other.m_data;
            Other.m_data = NULL;
            memcpy(m_tag, Other.m_tag, 4);
        }

        return *this;
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

    Type &operator[](size_t Index) const {
        return m_data[Index];
    }

    Type &operator[](size_t Index) {
        return m_data[Index];
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function creates a move reference, ready for our move constructor.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     Move reference to the pointer.
     *-----------------------------------------------------------------------------------------------*/
    ScopePtr &&Move(void) {
        return (ScopePtr &&)*this;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function resets the current pointer, decreasing the RC before the scope's end.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Reset(void) {
        this->~ScopePtr();
        m_data = NULL;
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

#endif /* _CXX_PTR_HXX_ */
