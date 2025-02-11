/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/mi.h>
#include <kernel/vid.h>
#include <string.h>

extern RtDList MiPoolFreeTagListHead;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries searching for the tracker for the given tag.
 *
 * PARAMETERS:
 *     Tag - Which tag we're trying to update the tracker for.
 *
 * RETURN VALUE:
 *     Either a pointer to the MiPoolTrackerHeader struct, or NULL if we didn't find it.
 *-----------------------------------------------------------------------------------------------*/
static MiPoolTrackerHeader *FindTracker(const char Tag[4]) {
    /* Will we have enough tags that a balanced tree would make a difference? */
    for (RtDList *ListHeader = MiPoolTagListHead.Next; ListHeader != &MiPoolTagListHead;
         ListHeader = ListHeader->Next) {
        MiPoolTrackerHeader *Header =
            CONTAINING_RECORD(ListHeader, MiPoolTrackerHeader, ListHeader);
        if (!memcmp(Header->Tag, Tag, 4)) {
            return Header;
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries updating the tag tracker list with a new allocation, possibly allocating
 *     an extra pool page if required.
 *
 * PARAMETERS:
 *     Size - How many bytes the allocation has; This should be the rounded up size (according to
 *            the small or the big allocation block size)!
 *     Tag - Which tag we're trying to update/create the tracker for.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiAddPoolTracker(size_t Size, const char Tag[4]) {
    /* Check if we already have a tag entry where to insert this allocation. */
    MiPoolTrackerHeader *MatchingHeader = FindTracker(Tag);

    /* Otherwise, hopefully we have empty tags waiting to be used? */
    if (!MatchingHeader) {
        RtDList *ListHeader = RtPopDList(&MiPoolFreeTagListHead);
        if (ListHeader != &MiPoolFreeTagListHead) {
            MatchingHeader = CONTAINING_RECORD(ListHeader, MiPoolTrackerHeader, ListHeader);
            memcpy(MatchingHeader->Tag, Tag, 4);
            RtAppendDList(&MiPoolTagListHead, &MatchingHeader->ListHeader);
        }
    }

    /* If we don't, do one last attempt at allocating a whole page for N new tags (where N is the
     * size of a page / size of a tag tracker); If we can't, just bail out without doing anything
     * (not a fatal error, just an inconvinence for debugging). */
    if (!MatchingHeader) {
        MiPoolTrackerHeader *Headers = MiAllocatePoolPages(1);
        if (!Headers) {
            VidPrint(
                VID_MESSAGE_ERROR,
                "Kernel Pool",
                "failed to allocate the pool tracker for \"%4s\"\n",
                Tag);
            return;
        }

        /* Cleanup everything by default, and put it all in the free list. */
        memset(Headers, 0, MM_PAGE_SIZE);
        for (uint64_t i = 0; i < MM_PAGE_SIZE / sizeof(MiPoolTrackerHeader); i++) {
            RtAppendDList(&MiPoolFreeTagListHead, &Headers[i].ListHeader);
        }

        /* Try to grab the pool tracker to the pool tracker allocations themselves; This free list
         * now is guaranteed to not be empty, so we also grab from it if it's the first pool tracker
         * allocation. */
        MiPoolTrackerHeader *TrackerHeader = FindTracker(MM_POOL_TAG_POOL);
        if (!TrackerHeader) {
            TrackerHeader = CONTAINING_RECORD(
                RtPopDList(&MiPoolFreeTagListHead), MiPoolTrackerHeader, ListHeader);
            memcpy(TrackerHeader->Tag, MM_POOL_TAG_POOL, 4);
            RtAppendDList(&MiPoolTagListHead, &TrackerHeader->ListHeader);
        }

        /* And update all stats for it; We never free tag allocations, so it only goes up. */
        TrackerHeader->Allocations++;
        TrackerHeader->AllocatedBytes += MM_PAGE_SIZE;
        TrackerHeader->MaxAllocations++;
        TrackerHeader->MaxAllocatedBytes += MM_PAGE_SIZE;

        /* Now we just need to grab the new tracker for the caller's tag (again, there should still
         * be stuff in the free list). */
        MatchingHeader =
            CONTAINING_RECORD(RtPopDList(&MiPoolFreeTagListHead), MiPoolTrackerHeader, ListHeader);
        memcpy(MatchingHeader->Tag, Tag, 4);
        RtAppendDList(&MiPoolTagListHead, &MatchingHeader->ListHeader);
    }

    MatchingHeader->Allocations++;
    MatchingHeader->AllocatedBytes += Size;

    if (MatchingHeader->Allocations > MatchingHeader->MaxAllocations) {
        MatchingHeader->MaxAllocations = MatchingHeader->Allocations;
    }

    if (MatchingHeader->AllocatedBytes > MatchingHeader->MaxAllocatedBytes) {
        MatchingHeader->MaxAllocatedBytes = MatchingHeader->AllocatedBytes;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries updating the tag tracker list with a now freed allocation.
 *
 * PARAMETERS:
 *     Size - How many bytes the allocation had; This should be the rounded up size (according to
 *            the small or the big allocation block size)!
 *     Tag - Which tag we're trying to update/create the tracker for.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiRemovePoolTracker(size_t Size, const char Tag[4]) {
    /* And don't bother with tag specific if MiAddPoolTracker failed to allocate the tag tracker
     * last time. */
    MiPoolTrackerHeader *MatchingHeader = FindTracker(Tag);
    if (MatchingHeader) {
        MatchingHeader->Allocations--;
        MatchingHeader->AllocatedBytes -= Size;
    }
}
