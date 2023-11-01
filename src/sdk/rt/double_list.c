/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <rt.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes a doubly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtInitializeDoublyLinkedList(RtDoublyLinkedListEntry *Head) {
    if (Head) {
        Head->Next = Head;
        Head->Prev = Head;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts a new entry at the front of a doubly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *     Entry - What we're inserting.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtPushDoublyLinkedList(RtDoublyLinkedListEntry *Head, RtDoublyLinkedListEntry *Entry) {
    if (Head && Entry) {
        Entry->Next = Head->Next;
        Entry->Prev = Head;
        Head->Next->Prev = Entry;
        Head->Next = Entry;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts a new entry at the tail of a doubly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *     Entry - What we're inserting.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtAppendDoublyLinkedList(RtDoublyLinkedListEntry *Head, RtDoublyLinkedListEntry *Entry) {
    if (Head && Entry) {
        Entry->Next = Head;
        Entry->Prev = Head->Prev;
        Head->Prev->Next = Entry;
        Head->Prev = Entry;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the entry at the front of a doubly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *
 * RETURN VALUE:
 *     NULL if the list was entry, what we removed otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtDoublyLinkedListEntry *RtPopDoublyLinkedList(RtDoublyLinkedListEntry *Head) {
    if (!Head) {
        return NULL;
    }

    RtDoublyLinkedListEntry *Entry = Head->Next;
    Head->Next = Entry->Next;
    Entry->Prev = Head;

    return Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the entry at the tail of a doubly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *
 * RETURN VALUE:
 *     NULL if the list was entry, what we removed otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtDoublyLinkedListEntry *RtTruncateDoublyLinkedList(RtDoublyLinkedListEntry *Head) {
    if (!Head) {
        return NULL;
    }

    RtDoublyLinkedListEntry *Entry = Head->Prev;
    Head->Prev = Entry->Prev;
    Entry->Next = Head;

    return Entry;
}
