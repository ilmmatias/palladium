/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_ARRAY_HXX_
#define _CXX_ARRAY_HXX_

#include <mm.h>
#include <string.h>

#include <cxx/new.hxx>

template <typename Type>
class AutoArray {
   public:
    AutoArray() {
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new heap-allocated array with automatic reference counting.
     *     This should prevent leaks (as long as you don't create any circular references), so it is
     *     the recommended way to do things in C++!
     *     For pointers that need no sharing (no reference counting, should always be dropped at
     *     the end of the scope), use ScopeArray<Type> instead!
     *
     * PARAMETERS:
     *     Tag - Which tag should be associated with this allocation.
     *     Elements - How many elements the array has.
     *
     * RETURN VALUE:
     *     Reference counted pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    AutoArray(const char Tag[4], size_t Elements) {
        void *Data = MmAllocatePool(Elements * sizeof(Type), Tag);
        if (!Data) {
            return;
        }

        m_references = (int *)MmAllocatePool(sizeof(int), "Arc ");
        if (!m_references) {
            MmFreePool(Data, Tag);
            return;
        }

        m_data = new (Data) Type[Elements];
        m_elements = Elements;
        *m_references = 1;
        memcpy(m_tag, Tag, 4);
    }

    AutoArray(AutoArray &&Other) {
        m_data = Other.m_data;
        m_elements = Other.m_elements;
        m_references = Other.m_references;
        Other.m_data = NULL;
        Other.m_elements = 0;
        Other.m_references = NULL;
        memcpy(m_tag, Other.m_tag, 4);
    }

    AutoArray(const AutoArray &Other) {
        m_data = Other.m_data;
        m_elements = Other.m_elements;
        m_references = Other.m_references;
        __atomic_add_fetch(m_references, 1, __ATOMIC_SEQ_CST);
        memcpy(m_tag, Other.m_tag, 4);
    }

    ~AutoArray(void) {
        /* Checking for exact zero should prevent multiple threads accidentally freeing the
         * data at the same time. */
        if (m_references && __atomic_sub_fetch(m_references, 1, __ATOMIC_SEQ_CST) == 0) {
            for (size_t i = 0; i < m_elements; i++) {
                m_data[i].~Type();
            }

            MmFreePool(m_references, "Arc ");
            MmFreePool(m_data, m_tag);
        }
    }

    AutoArray &operator=(const AutoArray &Other) {
        if (this != &Other) {
            this->~AutoArray();
            m_data = Other.m_data;
            m_elements = Other.m_elements;
            m_references = Other.m_references;
            __atomic_add_fetch(m_references, 1, __ATOMIC_SEQ_CST);
            memcpy(m_tag, Other.m_tag, 4);
        }

        return *this;
    }

    AutoArray &operator=(AutoArray &&Other) {
        if (this != &Other) {
            this->~AutoArray();
            m_data = Other.m_data;
            m_elements = Other.m_elements;
            m_references = Other.m_references;
            Other.m_data = NULL;
            Other.m_elements = NULL;
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

    const Type &operator[](size_t Index) const {
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
    AutoArray &&Move(void) {
        return (AutoArray &&)*this;
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
        this->~AutoArray();
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

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function returns how many elements this array has.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     How many elements we have.
     *-----------------------------------------------------------------------------------------------*/
    size_t Size(void) const {
        return m_elements;
    }

   private:
    Type *m_data;
    size_t m_elements;
    int *m_references;
    char m_tag[4];
};

template <typename Type>
class ScopeArray {
   public:
    ScopeArray() {
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a new heap-allocated array with an auto delete at the end of
     *     the scope. If you need sharing (with reference counting), use AutoArray<Type> instead!
     *
     * PARAMETERS:
     *     Tag - Which tag should be associated with this allocation.
     *     Elements - How many elements the array has.
     *
     * RETURN VALUE:
     *     Wrapped pointer; Make sure to check if the allocation didn't fail (either with
     *     operator!, or with .Data()), as we don't throw.
     *-----------------------------------------------------------------------------------------------*/
    ScopeArray(const char Tag[4], size_t Elements) {
        void *Data = MmAllocatePool(Elements * sizeof(Type), Tag);
        if (Data) {
            m_data = new (Data) Type[Elements];
            m_elements = Elements;
            memcpy(m_tag, Tag, 4);
        }
    }

    ScopeArray(ScopeArray &&Other) {
        m_data = Other.m_data;
        m_elements = Other.m_elements;
        Other.m_data = NULL;
        Other.m_elements = 0;
        memcpy(m_tag, Other.m_tag, 4);
    }

    /* Only valid in this scope, we can't let anyone copy the pointer! */
    ScopeArray(const ScopeArray &Other) = delete;
    ScopeArray &operator=(const ScopeArray &Other) = delete;

    ~ScopeArray(void) {
        if (m_data) {
            for (size_t i = 0; i < m_elements; i++) {
                m_data[i].~Type();
            }

            MmFreePool(m_data, m_tag);
        }
    }

    ScopeArray &operator=(ScopeArray &&Other) {
        if (this != &Other) {
            this->~ScopeArray();
            m_data = Other.m_data;
            m_elements = Other.m_elements;
            Other.m_data = NULL;
            Other.m_elements = 0;
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

    const Type &operator[](size_t Index) const {
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
    ScopeArray &&Move(void) {
        return (ScopeArray &&)*this;
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
        this->~ScopeArray();
        m_data = NULL;
        m_elements = 0;
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

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function returns how many elements this array has.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     How many elements we have.
     *-----------------------------------------------------------------------------------------------*/
    size_t Size(void) const {
        return m_elements;
    }

   private:
    Type *m_data;
    size_t m_elements;
    char m_tag[4];
};

#endif /* _CXX_ARRAY_HXX_ */
