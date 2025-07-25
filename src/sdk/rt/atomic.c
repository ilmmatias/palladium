/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <rt/atomic.h>
#include <rt/except.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function inserts a new entry into an atomic/interlocked singly linked list.
 *
 * PARAMETERS:
 *     Header - Header of the list.
 *     Entry - What we're inserting.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void RtPushAtomicSList(RtAtomicSList *Header, RtSList *Entry) {
    RtAtomicSList OldHeader, NewHeader;
    __atomic_load(Header, &OldHeader, __ATOMIC_ACQUIRE);
    do {
        Entry->Next = OldHeader.Next;
        NewHeader.Next = Entry;
        NewHeader.Tag = OldHeader.Tag + 1;
    } while (!__atomic_compare_exchange(
        Header, &OldHeader, &NewHeader, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE));
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Helper function to fetch the next head from inside an atomic list header; This is an unsafe
 *     operation (because ABA), and should be ran under SEH.
 *
 * PARAMETERS:
 *     Header - Header of the list.
 *
 * RETURN VALUE:
 *     Next head for the list.
 *-----------------------------------------------------------------------------------------------*/
static RtSList *FetchNextHead(RtAtomicSList *Header) {
    return Header->Next->Next;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function removes the top of the specified atomic/interlocked singly linked list.
 *
 * PARAMETERS:
 *     Header - Header entry of the list.
 *
 * RETURN VALUE:
 *     NULL if the list was entry, what we removed otherwise.
 *-----------------------------------------------------------------------------------------------*/
RtSList *RtPopAtomicSList(RtAtomicSList *Header) {
    RtAtomicSList OldHeader, NewHeader;

    int RemainingAttempts = 1024;
    while (--RemainingAttempts > 0) {
        __atomic_load(Header, &OldHeader, __ATOMIC_ACQUIRE);

        do {
            if (!OldHeader.Next) {
                return NULL;
            }

            /* Dereferencing the old pointer needs to be done under an SEH block, as someone else
             * might have preempted us, and freed the pointer before we had a chance to dereference
             * (though this should be rare enough); Just make sure to limit the amount of retries
             * (using the RemainingAttempts variable above). */
            __try {
                NewHeader.Next = FetchNextHead(&OldHeader);
            } __except (
                _exception_code() == RT_EXC_ACCESS_VIOLATION ? RT_EXC_EXECUTE_HANDLER
                                                             : RT_EXC_CONTINUE_SEARCH) {
                continue;
            }

            NewHeader.Tag = OldHeader.Tag + 1;
        } while (!__atomic_compare_exchange(
            Header, &OldHeader, &NewHeader, 0, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE));

        OldHeader.Next->Next = NULL;
        return OldHeader.Next;
    }

    return NULL;
}
