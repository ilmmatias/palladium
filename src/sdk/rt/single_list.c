/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <rt.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts a new entry into a singly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *     Entry - What we're inserting.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtPushSinglyLinkedList(RtSinglyLinkedListEntry *Head, RtSinglyLinkedListEntry *Entry) {
    if (Head && Entry) {
        Entry->Next = Head->Next;
        Head->Next = Entry;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the last inserted entry from a singly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *
 * RETURN VALUE:
 *     NULL if the list was entry, what we removed otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtSinglyLinkedListEntry *RtPopSinglyLinkedList(RtSinglyLinkedListEntry *Head) {
    if (!Head || !Head->Next) {
        return NULL;
    }

    RtSinglyLinkedListEntry *Entry = Head->Next;
    Head->Next = Entry->Next;
    Entry->Next = NULL;

    return Entry;
}
