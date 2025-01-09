/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _CXX_LIST_HXX_
#define _CXX_LIST_HXX_

#include <mm.h>
#include <rt/list.h>
#include <string.h>

#include <cxx/new.hxx>

template <typename Type>
class SList {
   public:
    struct ConstIterator {
        ConstIterator(RtSList *Item) : m_item(Item) {
        }

        template <typename PointerType>
        operator PointerType *(void) const {
            return (PointerType *)&CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        const Type &operator*(void) const {
            return CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        const Type *operator->(void) const {
            return &CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        ConstIterator &operator++(void) {
            if (m_item) {
                m_item = m_item->Next;
            }

            return *m_item;
        }

        ConstIterator operator++(int) {
            ConstIterator result = *this;

            if (m_item) {
                m_item = m_item->Next;
            }

            return result;
        }

        operator bool(void) const {
            return m_item != NULL;
        }

        bool operator!(void) const {
            return !m_item;
        }

        bool operator==(const ConstIterator &Other) const {
            return m_item == Other.m_item;
        }

        bool operator!=(const ConstIterator &Other) const {
            return m_item != Other.m_item;
        }

       private:
        RtSList *m_item;
    };

    struct MutIterator {
        MutIterator(RtSList *Item) : m_item(Item) {
        }

        template <typename PointerType>
        operator PointerType *(void) const {
            return (PointerType *)&CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        Type &operator*(void) {
            return CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        Type *operator->(void) {
            return &CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        MutIterator &operator++(void) {
            if (m_item) {
                m_item = m_item->Next;
            }

            return *m_item;
        }

        MutIterator operator++(int) {
            MutIterator result = *this;

            if (m_item) {
                m_item = m_item->Next;
            }

            return result;
        }

        operator bool(void) const {
            return m_item != NULL;
        }

        bool operator!(void) const {
            return !m_item;
        }

        bool operator==(const MutIterator &Other) const {
            return m_item == Other.m_item;
        }

        bool operator!=(const MutIterator &Other) const {
            return m_item != Other.m_item;
        }

       private:
        RtSList *m_item;
    };

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a C++ managed singly linked list.
     *
     * PARAMETERS:
     *     Tag - Which tag to use for the node allocations.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    SList(const char Tag[4]) {
        memcpy(m_tag, Tag, 4);
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function allocates a new node and calls the C++ constructor for the Type class,
     *     before doing an RtPushSList. Move constructor version, use this for scope pointers or
     *     other things that you can't/shouldn't use the copy constructor.
     *
     * PARAMETERS:
     *     Data - What to pass into the move constructor.
     *
     * RETURN VALUE:
     *     1 on success, 0 otherwise.
     *-----------------------------------------------------------------------------------------------*/
    int Push(Type &&Data) {
        void *LeafSpace = MmAllocatePool(sizeof(Node), m_tag);

        if (LeafSpace) {
            Node *Leaf = new (LeafSpace) Node((Type &&)Data);
            RtPushSList(&m_head, &Leaf->Header);
            m_size++;
        }

        return LeafSpace != NULL;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function allocates a new node and calls the C++ constructor for the Type class,
     *     before doing an RtPushSList.
     *
     * PARAMETERS:
     *     Data - What to pass into the copy constructor.
     *
     * RETURN VALUE:
     *     1 on success, 0 otherwise.
     *-----------------------------------------------------------------------------------------------*/
    int Push(const Type &Data) {
        void *LeafSpace = MmAllocatePool(sizeof(Node), m_tag);

        if (LeafSpace) {
            Node *Leaf = new (LeafSpace) Node(Data);
            RtPushSList(&m_head, &Leaf->Header);
            m_size++;
        }

        return LeafSpace != NULL;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function removes the top of the list, automatically calling the deconstructor for
     *     the Type class.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Pop(void) {
        RtSList *Result = RtPopSList(&m_head);
        if (Result) {
            Node *Leaf = CONTAINING_RECORD(Result, Node, Header);
            Leaf->~Node();
            MmFreePool(Leaf, m_tag);
            m_size--;
        }
    }

    Type *First(void) {
        return CONTAINING_RECORD(m_head.Next, Node, Header)->Data;
    }

    const Type *First(void) const {
        return CONTAINING_RECORD(m_head.Next, Node, Header)->Data;
    }

    ConstIterator Iterator(void) const {
        return ConstIterator(m_head.Next);
    }

    MutIterator Iterator(void) {
        return MutIterator(m_head.Next);
    }

    size_t Size(void) const {
        return m_size;
    }

   private:
    struct Node {
        Node(const Type &Data) : Data(Data) {
        }

        Node(Type &&Data) : Data((Type &&)Data) {
        }

        RtSList Header;
        Type Data;
    };

    char m_tag[4];
    size_t m_size;
    RtSList m_head;
};

template <typename Type>
class DList {
   public:
    struct ConstIterator {
        ConstIterator(RtDList *Head, RtDList *Item, int Direction)
            : m_head(Head), m_item(Item), m_direction(Direction) {
        }

        template <typename PointerType>
        operator PointerType *(void) const {
            return (PointerType *)&CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        const Type &operator*(void) const {
            return CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        const Type *operator->(void) const {
            return &CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        ConstIterator &operator++(void) {
            if (m_item != m_head) {
                m_item = m_item->Next;
            }

            return *m_item;
        }

        ConstIterator operator++(int) {
            ConstIterator result = *this;

            if (m_item != m_head) {
                m_item = m_item->Next;
            }

            return result;
        }

        operator bool(void) const {
            return m_item != m_head;
        }

        bool operator!(void) const {
            return m_item == m_head;
        }

        bool operator==(const ConstIterator &Other) const {
            return m_item == Other.m_item;
        }

        bool operator!=(const ConstIterator &Other) const {
            return m_item != Other.m_item;
        }

       private:
        RtDList *m_head;
        RtDList *m_item;
        int m_direction;
    };

    struct MutIterator {
        MutIterator(RtDList *Head, RtDList *Item, int Direction)
            : m_head(Head), m_item(Item), m_direction(Direction) {
        }

        template <typename PointerType>
        operator PointerType *(void) const {
            return (PointerType *)&CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        Type &operator*(void) {
            return CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        Type *operator->(void) {
            return &CONTAINING_RECORD(m_item, Node, Header)->Data;
        }

        MutIterator &operator++(void) {
            if (m_item != m_head) {
                if (m_direction) {
                    m_item = m_item->Prev;
                } else {
                    m_item = m_item->Next;
                }
            }

            return *m_item;
        }

        MutIterator operator++(int) {
            MutIterator result = *this;

            if (m_item != m_head) {
                if (m_direction) {
                    m_item = m_item->Prev;
                } else {
                    m_item = m_item->Next;
                }
            }

            return result;
        }

        operator bool(void) const {
            return m_item != m_head;
        }

        bool operator!(void) const {
            return m_item == m_head;
        }

        bool operator==(const MutIterator &Other) const {
            return m_item == Other.m_item;
        }

        bool operator!=(const MutIterator &Other) const {
            return m_item != Other.m_item;
        }

       private:
        RtDList *m_head;
        RtDList *m_item;
        int m_direction;
    };

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function initializes a C++ managed doubly linked list.
     *
     * PARAMETERS:
     *     Tag - Which tag to use for the node allocations.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    DList(const char Tag[4]) {
        RtInitializeDList(&m_head);
        memcpy(m_tag, Tag, 4);
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function allocates a new node and calls the C++ constructor for the Type class,
     *     before prepending it. Move constructor version, use this for scope pointers or other
     *     things that you can't/shouldn't use the copy constructor.
     *
     * PARAMETERS:
     *     Data - What to pass into the move constructor.
     *
     * RETURN VALUE:
     *     1 on success, 0 otherwise.
     *-----------------------------------------------------------------------------------------------*/
    int Push(Type &&Data) {
        void *LeafSpace = MmAllocatePool(sizeof(Node), m_tag);

        if (LeafSpace) {
            Node *Leaf = new (LeafSpace) Node((Type &&)Data);
            RtPushDList(&m_head, &Leaf->Header);
            m_size++;
        }

        return LeafSpace != NULL;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function allocates a new node and calls the C++ constructor for the Type class,
     *     before prepending it.
     *
     * PARAMETERS:
     *     Data - What to pass into the copy constructor.
     *
     * RETURN VALUE:
     *     1 on success, 0 otherwise.
     *-----------------------------------------------------------------------------------------------*/
    int Push(const Type &Data) {
        void *LeafSpace = MmAllocatePool(sizeof(Node), m_tag);

        if (LeafSpace) {
            Node *Leaf = new (LeafSpace) Node(Data);
            RtPushDList(&m_head, &Leaf->Header);
            m_size++;
        }

        return LeafSpace != NULL;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function allocates a new node and calls the C++ constructor for the Type class,
     *     before appending it. Move constructor version, use this for scope pointers or other
     *     things that you can't/shouldn't use the copy constructor.
     *
     * PARAMETERS:
     *     Data - What to pass into the move constructor.
     *
     * RETURN VALUE:
     *     1 on success, 0 otherwise.
     *-----------------------------------------------------------------------------------------------*/
    int Append(Type &&Data) {
        void *LeafSpace = MmAllocatePool(sizeof(Node), m_tag);

        if (LeafSpace) {
            Node *Leaf = new (LeafSpace) Node((Type &&)Data);
            RtAppendDList(&m_head, &Leaf->Header);
            m_size++;
        }

        return LeafSpace != NULL;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function allocates a new node and calls the C++ constructor for the Type class,
     *     before appending it.
     *
     * PARAMETERS:
     *     Data - What to pass into the copy constructor.
     *
     * RETURN VALUE:
     *     1 on success, 0 otherwise.
     *-----------------------------------------------------------------------------------------------*/
    int Append(const Type &Data) {
        void *LeafSpace = MmAllocatePool(sizeof(Node), m_tag);

        if (LeafSpace) {
            Node *Leaf = new (LeafSpace) Node(Data);
            RtAppendDList(&m_head, &Leaf->Header);
            m_size++;
        }

        return LeafSpace != NULL;
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function removes the head of the list, automatically calling the deconstructor for
     *     the Type class.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Pop(void) {
        RtDList *Result = RtPopDList(&m_head);
        if (Result) {
            Node *Leaf = CONTAINING_RECORD(Result, Node, Header);
            Leaf->~Node();
            MmFreePool(Leaf, m_tag);
            m_size--;
        }
    }

    /*-------------------------------------------------------------------------------------------------
     * PURPOSE:
     *     This function removes the tail of the list, automatically calling the deconstructor for
     *     the Type class.
     *
     * PARAMETERS:
     *     None.
     *
     * RETURN VALUE:
     *     None.
     *-----------------------------------------------------------------------------------------------*/
    void Truncate(void) {
        RtDList *Result = RtTruncateDList(&m_head);
        if (Result) {
            Node *Leaf = CONTAINING_RECORD(Result, Node, Header);
            Leaf->~Node();
            MmFreePool(Leaf, m_tag);
            m_size--;
        }
    }

    Type *First(void) {
        return m_head.Next == &m_head ? NULL : &CONTAINING_RECORD(m_head.Next, Node, Header)->Data;
    }

    const Type *First(void) const {
        return m_head.Next == &m_head ? NULL : &CONTAINING_RECORD(m_head.Next, Node, Header)->Data;
    }

    Type *Last(void) {
        return m_head.Prev == &m_head ? NULL : &CONTAINING_RECORD(m_head.Prev, Node, Header)->Data;
    }

    const Type *Last(void) const {
        return m_head.Prev == &m_head ? NULL : &CONTAINING_RECORD(m_head.Prev, Node, Header)->Data;
    }

    ConstIterator Iterator(void) const {
        return ConstIterator(&m_head, m_head.Next, 0);
    }

    MutIterator Iterator(void) {
        return MutIterator(&m_head, m_head.Next, 0);
    }

    ConstIterator ReverseIterator(void) const {
        return ConstIterator(&m_head, m_head.Prev, 1);
    }

    MutIterator ReverseIterator(void) {
        return MutIterator(&m_head, m_head.Prev, 1);
    }

    size_t Size(void) const {
        return m_size;
    }

   private:
    struct Node {
        Node(const Type &Data) : Data(Data) {
        }

        Node(Type &&Data) : Data((Type &&)Data) {
        }

        RtDList Header;
        Type Data;
    };

    char m_tag[4];
    size_t m_size;
    RtDList m_head;
};

#endif /* _CXX_LIST_HXX_ */
