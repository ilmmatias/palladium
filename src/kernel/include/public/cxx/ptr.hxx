/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_PTR_HXX_
#define _CXX_PTR_HXX_

#include <mm.h>
#include <string.h>

namespace impl {
template <typename Type>
class AutoPtr {
   public:
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

   protected:
    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new pointer with automatic reference counting.
     *     This should prevent leaks (as long as you don't create any circular references), so it is
     *     the recommended way to do things in C++!
     *     For pointers that need no sharing (no reference counting, should always be dropped at
     *     the end of the scope), use ScopePtr<Type> instead!
     *
     * PARAMETERS:
     *     Elements - How many elements we should allocate; Set this to 1 if we're not an array.
     *     Tag - Which tag should be associated with this allocation.
     *
     * RETURN VALUE:
     *     Reference counted pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    AutoPtr(size_t Elements, const char Tag[4]) {
        m_data = (Type *)MmAllocatePool(Elements * sizeof(Type), Tag);
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

    AutoPtr(AutoPtr &&Other) {
        m_data = Other.m_data;
        Other.m_data = NULL;
        m_references = Other.m_references;
        Other.m_references = NULL;
        memcpy(m_tag, Other.m_tag, 4);
    }

    Type *m_data;
    char m_tag[4];
    int *m_references;
};

template <typename Type>
class ScopePtr {
   public:
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

   protected:
    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new pointer with an auto delete at the end of the scope.
     *     For pointers that need sharing (with reference counting), use AutoPtr<Type> instead!
     *
     * PARAMETERS:
     *     Elements - How many elements we should allocate; Set this to 1 if we're not an array.
     *     Tag - Which tag should be associated with this allocation.
     *
     * RETURN VALUE:
     *     Wrapped pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    ScopePtr(size_t Elements, const char Tag[4]) {
        m_data = (Type *)MmAllocatePool(Elements * sizeof(Type), Tag);
        memcpy(m_tag, Tag, 4);
    }

    ScopePtr(ScopePtr &&Other) {
        m_data = Other.m_data;
        Other.m_data = NULL;
        memcpy(m_tag, Other.m_tag, 4);
    }

    Type *m_data;
    char m_tag[4];
};
}  // namespace impl

/* Public instantiations; They all manually define the usual and move constructors, redirecting
 * to the protected base fns. */

template <typename Type>
class AutoPtr : public impl::AutoPtr<Type> {
   public:
    AutoPtr(const char Tag[4]) : impl::AutoPtr<Type>(1, Tag) {
    }

    AutoPtr(impl::AutoPtr<Type> &&Other) : impl::AutoPtr<Type>((AutoPtr &&)Other) {
    }
};

template <typename Type>
class AutoPtr<Type[]> : public impl::AutoPtr<Type> {
   public:
    AutoPtr(size_t Elements, const char Tag[4]) : impl::AutoPtr<Type>(Elements, Tag) {
    }

    AutoPtr(impl::AutoPtr<Type> &&Other) : impl::AutoPtr<Type>((AutoPtr &&)Other) {
    }
};

template <typename Type, size_t Elements>
class AutoPtr<Type[Elements]> : public impl::AutoPtr<Type> {
   public:
    AutoPtr(const char Tag[4]) : impl::AutoPtr<Type>(Elements, Tag) {
    }

    AutoPtr(impl::AutoPtr<Type> &&Other) : impl::AutoPtr<Type>((AutoPtr &&)Other) {
    }
};

template <typename Type>
class ScopePtr : public impl::ScopePtr<Type> {
   public:
    ScopePtr(const char Tag[4]) : impl::ScopePtr<Type>(1, Tag) {
    }

    ScopePtr(impl::ScopePtr<Type> &&Other) : impl::ScopePtr<Type>((ScopePtr &&)Other) {
    }
};

template <typename Type>
class ScopePtr<Type[]> : public impl::ScopePtr<Type> {
   public:
    ScopePtr(size_t Elements, const char Tag[4]) : impl::ScopePtr<Type>(Elements, Tag) {
    }

    ScopePtr(impl::ScopePtr<Type> &&Other) : impl::ScopePtr<Type>((ScopePtr &&)Other) {
    }
};

template <typename Type, size_t Elements>
class ScopePtr<Type[Elements]> : public impl::ScopePtr<Type> {
   public:
    ScopePtr(const char Tag[4]) : impl::ScopePtr<Type>(Elements, Tag) {
    }

    ScopePtr(impl::ScopePtr<Type> &&Other) : impl::ScopePtr<Type>((ScopePtr &&)Other) {
    }
};

#endif /* _CXX_PTR_HXX_ */
