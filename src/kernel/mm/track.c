/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>
#include <kernel/mi.h>
#include <kernel/vid.h>
#include <string.h>

static RtSList FreeList = {};
static KeSpinLock FreeListLock = {0};
static KeSpinLock TagListLock[256] = {0};
static MiPoolTrackerHeader *PoolTracker = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the tag tracking support on the kernel pool.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePoolTracker(void) {
    /* The initial tracker (which tracks the pool allocations themselves) is considered not
     * optional, and we'll just panic if it fails to be allocated. There should be no need to hold
     * the tag lock here, as SMP support isn't online yet. */
    MiPoolTrackerHeader *Headers = MiAllocatePoolPages(1);
    if (!Headers) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_POOL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    /* Cleanup everything and add it all to the free tag list. */
    memset(Headers, 0, MM_PAGE_SIZE);
    for (uint64_t i = 0; i < MM_PAGE_SIZE / sizeof(MiPoolTrackerHeader); i++) {
        RtPushSList(&FreeList, &Headers[i].ListHeader);
    }

    /* Followed by grabbing (and initialzing) the pool tracker itself. */
    uint8_t PoolTagHash = MiGetTagHash(MM_POOL_TAG_POOL);
    PoolTracker = CONTAINING_RECORD(RtPopSList(&FreeList), MiPoolTrackerHeader, ListHeader);
    memcpy(PoolTracker->Tag, MM_POOL_TAG_POOL, 4);
    PoolTracker->Allocations = 1;
    PoolTracker->AllocatedBytes = MM_PAGE_SIZE;
    PoolTracker->MaxAllocations = 1;
    PoolTracker->MaxAllocatedBytes = MM_PAGE_SIZE;
    RtPushSList(&MiPoolTagListHead[PoolTagHash], &PoolTracker->ListHeader);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates the hash for a tag (that can be used to index into the tag list).
 *
 * PARAMETERS:
 *     Tag - Which tag we're hashing.
 *
 * RETURN VALUE:
 *     8-bit hash of the tag.
 *-----------------------------------------------------------------------------------------------*/
uint8_t MiGetTagHash(const char Tag[4]) {
    return Tag[0] * 29791 + Tag[1] * 961 + Tag[2] * 31 + Tag[3];
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries searching for the tracker for the given tag.
 *
 * PARAMETERS:
 *     Tag - Which tag we're trying to update the tracker for.
 *     Hash - Hashed version of the tag (passed as a parameter to prevent calculating it multiple
 *            times).
 *
 * RETURN VALUE:
 *     Either a pointer to the MiPoolTrackerHeader struct, or NULL if we didn't find it.
 *-----------------------------------------------------------------------------------------------*/
MiPoolTrackerHeader *MiFindTracker(const char Tag[4]) {
    /* Will we have enough tags that a balanced tree would make a difference? */
    uint8_t Hash = MiGetTagHash(Tag);
    for (RtSList *ListHeader = MiPoolTagListHead[Hash].Next; ListHeader;
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
 *     an extra pool page if required. We expect to be called at DISPATCH IRQL.
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
    uint8_t Hash = MiGetTagHash(Tag);
    MiPoolTrackerHeader *Match = MiFindTracker(Tag);

    /* If we don't get ready by locking this hash bucket for modification (and making sure someone
     * didn't add this tag to it first as we were waiting for the lock). */
    if (!Match) {
        KeAcquireSpinLockAtCurrentIrql(&TagListLock[Hash]);
        Match = MiFindTracker(Tag);
        if (Match) {
            KeReleaseSpinLockAtCurrentIrql(&TagListLock[Hash]);
        }
    }

    /* Otherwise, hopefully we have empty tags waiting to be used? */
    if (!Match) {
        KeAcquireSpinLockAtCurrentIrql(&FreeListLock);
        RtSList *ListHeader = RtPopSList(&FreeList);
        KeReleaseSpinLockAtCurrentIrql(&FreeListLock);
        if (ListHeader) {
            Match = CONTAINING_RECORD(ListHeader, MiPoolTrackerHeader, ListHeader);
            memcpy(Match->Tag, Tag, 4);
            RtAtomicPushSList(&MiPoolTagListHead[Hash], &Match->ListHeader);
            KeReleaseSpinLockAtCurrentIrql(&TagListLock[Hash]);
        }
    }

    /* If we don't, do one last attempt at allocating a whole page for N new tags (where N is the
     * size of a page / size of a tag tracker); If we can't, just bail out without doing anything
     * (not a fatal error, just an inconvinence for debugging). */
    if (!Match) {
        MiPoolTrackerHeader *Headers = MiAllocatePoolPages(1);
        if (!Headers) {
            KeReleaseSpinLockAtCurrentIrql(&TagListLock[Hash]);
            VidPrint(
                VID_MESSAGE_ERROR,
                "Kernel Pool",
                "failed to allocate the pool tracker for \"%4s\"\n",
                Tag);
            return;
        }

        /* Cleanup everything by default. */
        memset(Headers, 0, MM_PAGE_SIZE);

        /* Then lock the free list and add everything to it. */
        KeAcquireSpinLockAtCurrentIrql(&FreeListLock);
        for (uint64_t i = 0; i < MM_PAGE_SIZE / sizeof(MiPoolTrackerHeader); i++) {
            RtPushSList(&FreeList, &Headers[i].ListHeader);
        }

        /* Followed by grabbing the new tracker for the caller's tag (and releasing the lock). */
        Match = CONTAINING_RECORD(RtPopSList(&FreeList), MiPoolTrackerHeader, ListHeader);
        KeReleaseSpinLockAtCurrentIrql(&FreeListLock);
        memcpy(Match->Tag, Tag, 4);
        RtAtomicPushSList(&MiPoolTagListHead[Hash], &Match->ListHeader);
        KeReleaseSpinLockAtCurrentIrql(&TagListLock[Hash]);

        /* At last, we did a new pool related allocation, so we need to update the pool tracker
         * itself (it's guaranteed to always exist, and we always have a fixed pointer to it). */
        __atomic_fetch_add(&PoolTracker->Allocations, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&PoolTracker->AllocatedBytes, MM_PAGE_SIZE, __ATOMIC_RELAXED);
        __atomic_fetch_add(&PoolTracker->MaxAllocations, 1, __ATOMIC_RELAXED);
        __atomic_fetch_add(&PoolTracker->MaxAllocatedBytes, MM_PAGE_SIZE, __ATOMIC_RELAXED);
    }

    uint64_t Allocations = __atomic_add_fetch(&Match->Allocations, 1, __ATOMIC_RELAXED);
    uint64_t AllocatedBytes = __atomic_add_fetch(&Match->AllocatedBytes, Size, __ATOMIC_RELAXED);
    __atomic_fetch_max(&Match->MaxAllocations, Allocations, __ATOMIC_RELAXED);
    __atomic_fetch_max(&Match->MaxAllocatedBytes, AllocatedBytes, __ATOMIC_RELAXED);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries updating the tag tracker list with a now freed allocation. We expect to
 *     be called at DISPATCH IRQL.
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
    MiPoolTrackerHeader *Match = MiFindTracker(Tag);
    if (Match) {
        __atomic_fetch_sub(&Match->Allocations, 1, __ATOMIC_RELAXED);
        __atomic_fetch_sub(&Match->AllocatedBytes, Size, __ATOMIC_RELAXED);
    }
}
