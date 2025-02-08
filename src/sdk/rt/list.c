/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <rt/list.h>

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
void RtPushSList(RtSList *Head, RtSList *Entry) {
    Entry->Next = Head->Next;
    Head->Next = Entry;
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
RtSList *RtPopSList(RtSList *Head) {
    if (!Head->Next) {
        return NULL;
    }

    RtSList *Entry = Head->Next;
    Head->Next = Entry->Next;
    Entry->Next = NULL;

    return Entry;
}

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
void RtInitializeDList(RtDList *Head) {
    Head->Next = Head;
    Head->Prev = Head;
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
void RtPushDList(RtDList *Head, RtDList *Entry) {
    RtDList *Next = Head->Next;
    Entry->Next = Next;
    Entry->Prev = Head;
    Next->Prev = Entry;
    Head->Next = Entry;
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
void RtAppendDList(RtDList *Head, RtDList *Entry) {
    RtDList *Prev = Head->Prev;
    Entry->Next = Head;
    Entry->Prev = Prev;
    Prev->Next = Entry;
    Head->Prev = Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the entry at the front of a doubly linked list.
 *
 * PARAMETERS:
 *     Head - Header entry of the list.
 *
 * RETURN VALUE:
 *     The list header itself if the list was entry, what we removed otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtDList *RtPopDList(RtDList *Head) {
    RtDList *Entry = Head->Next;
    RtDList *Next = Entry->Next;
    Head->Next = Next;
    Next->Prev = Head;
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
 *     The list header itself if the list was entry, what we removed otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtDList *RtTruncateDList(RtDList *Head) {
    RtDList *Entry = Head->Prev;
    RtDList *Prev = Entry->Prev;
    Head->Prev = Prev;
    Prev->Next = Head;
    return Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the given entry from the doubly linked list it is currently associated
 *     with.
 *
 * PARAMETERS:
 *     Entry - What we're trying to remove.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtUnlinkDList(RtDList *Entry) {
    RtDList *Prev = Entry->Prev;
    RtDList *Next = Entry->Next;
    Prev->Next = Next;
    Next->Prev = Prev;
}
