/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <rt/bitmap.h>
#include <string.h>

typedef struct {
    RtSList ListHeader;
    char Tag[4];
    uint32_t Head;
} BlockHeader;

static RtSList FreeBlockList[MM_POOL_BLOCK_COUNT] = {};
static KeSpinLock FreeBlockLock[MM_POOL_BLOCK_COUNT] = {};

uint64_t *MiPoolBitmapBuffer = NULL;
RtBitmap MiPoolBitmap = {};
uint64_t MiPoolBitmapHint = 0;
RtSList MiPoolTagListHead[256] = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the bucket index for the given size.
 *
 * PARAMETERS:
 *     Size - Size in bytes.
 *
 * RETURN VALUE:
 *     Bucket index.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t GetHeadIndex(size_t Size) {
    if (Size < MM_POOL_SMALL_MAX) {
        return (Size - 1) >> MM_POOL_SMALL_SHIFT;
    } else if (Size < MM_POOL_MEDIUM_MAX) {
        return MM_POOL_SMALL_COUNT + ((Size - MM_POOL_MEDIUM_MIN - 1) >> MM_POOL_MEDIUM_SHIFT);
    } else {
        return MM_POOL_SMALL_COUNT + MM_POOL_MEDIUM_COUNT +
               ((Size - MM_POOL_LARGE_MIN - 1) >> MM_POOL_LARGE_SHIFT);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets the size of a specific bucket.
 *
 * PARAMETERS:
 *     Head - Bucket index.
 *
 * RETURN VALUE:
 *     Size in bytes of the specified bucket.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t GetHeadSize(uint32_t Head) {
    if (Head < MM_POOL_SMALL_COUNT) {
        return (Head + 1) << MM_POOL_SMALL_SHIFT;
    } else if (Head < MM_POOL_SMALL_COUNT + MM_POOL_MEDIUM_COUNT) {
        return MM_POOL_MEDIUM_MIN + ((Head - MM_POOL_SMALL_COUNT + 1) << MM_POOL_MEDIUM_SHIFT);
    } else {
        return MM_POOL_LARGE_MIN +
               ((Head - MM_POOL_SMALL_COUNT - MM_POOL_MEDIUM_COUNT + 1) << MM_POOL_LARGE_SHIFT);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gets how many pages should be used when allocating a new segment for a
 *     specific bucket size.
 *
 * PARAMETERS:
 *     Head - Bucket index.
 *
 * RETURN VALUE:
 *     Amount of pages that should be allocated.
 *-----------------------------------------------------------------------------------------------*/
static uint64_t GetHeadPages(uint32_t Head) {
    if (Head < MM_POOL_SMALL_COUNT) {
        return MM_POOL_SMALL_PAGES;
    } else if (Head < MM_POOL_SMALL_COUNT + MM_POOL_MEDIUM_COUNT) {
        return MM_POOL_MEDIUM_PAGES;
    } else {
        return MM_POOL_LARGE_PAGES;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the kernel pool allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MiInitializePool(void) {
    /* It doesn't make much sense to go over the physical memory limit in the pool, so, let's limit
     * it to 75% of the physical memory size (or the max pool size, whichever is smaller). */
    uint64_t MemorySize = MiTotalSystemPages * MM_PAGE_SIZE;
    uint64_t PoolSize = ((MemorySize * 75) / 100 + MM_PAGE_SIZE - 1) & ~(MM_PAGE_SIZE - 1);
    if (PoolSize > MI_POOL_MAX_SIZE) {
        PoolSize = MI_POOL_MAX_SIZE;
    }

    /* Grab some physical memory and map it for the pool bitmap. */
    uint64_t PoolPages = PoolSize >> MM_PAGE_SHIFT;
    uint64_t Size = ((PoolPages + 63) >> 6) << 3;
    uint64_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
    uint64_t PhysicalAddress = MiAllocateEarlyPages(Pages);
    if (!PhysicalAddress) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_POOL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    MiPoolBitmapBuffer = (uint64_t *)(MI_VIRTUAL_OFFSET + PhysicalAddress);
    if (!HalpMapContiguousPages(
            MiPoolBitmapBuffer, PhysicalAddress, Pages << MM_PAGE_SHIFT, MI_MAP_WRITE)) {
        KeFatalError(
            KE_PANIC_KERNEL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_POOL_INITIALIZATION_FAILURE,
            KE_PANIC_PARAMETER_OUT_OF_RESOURCES,
            0,
            0);
    }

    /* And clear up the bitmap (as, unlike MmAllocatePool, doing Allocate+MapPages doesn't clean the
     * memory). */
    RtInitializeBitmap(&MiPoolBitmap, MiPoolBitmapBuffer, PoolPages);
    RtClearAllBits(&MiPoolBitmap);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a block of memory of the specified size.
 *
 * PARAMETERS:
 *     Size - The size of the block to allocate.
 *     Tag - Name/identifier to be attached to the block.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is was no free entry and requesting
 *     a new page failed.
 *-----------------------------------------------------------------------------------------------*/
void *MmAllocatePool(size_t Size, const char Tag[4]) {
    /* We should just crash right away if we're above DISPATCH. */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);

    if (!Size) {
        Size = 1;
    }

    /* Anything higher than LARGE_SIZE is going into the underlying pool page allocator (and we
     * won't cache it). */
    if (Size > MM_POOL_LARGE_MAX) {
        uint32_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
        void *Base = MiAllocatePoolPages(Pages);

        if (Base) {
            /* Tag the allocation and try to account for it in the pool tracker. */
            memcpy(MI_PAGE_ENTRY(HalpGetPhysicalAddress(Base)).Tag, Tag, 4);
            MiAddPoolTracker(Pages << MM_PAGE_SHIFT, Tag);
            KeLowerIrql(OldIrql);
            memset(Base, 0, Pages << MM_PAGE_SHIFT);
        } else {
            KeLowerIrql(OldIrql);
        }

        return Base;
    }

    /* Otherwise, we need to discover the "head" (bucket index) of the allocation size, and then we
     * can try popping a free entry from the matching bucket (either from the local list, or from
     * the global list after acquiring its lock). */
    uint32_t Head = GetHeadIndex(Size);
    uint32_t HeadPages = GetHeadPages(Head);
    uint64_t HeadSize = GetHeadSize(Head);
    uint64_t FullSize = HeadSize + sizeof(BlockHeader);
    KeProcessor *Processor = KeGetCurrentProcessor();
    if (Processor->FreePoolBlockListHead[Head].Next) {
        BlockHeader *Header = CONTAINING_RECORD(
            RtPopSList(&Processor->FreePoolBlockListHead[Head]), BlockHeader, ListHeader);

        if (Header->Head != Head) {
            KeFatalError(KE_PANIC_BAD_POOL_HEADER, (uint64_t)Header, Header->Head, Head, 0);
        }

        Processor->FreePoolBlockListSize[Head]--;

        memcpy(Header->Tag, Tag, 4);
        MiAddPoolTracker(FullSize, Tag);
        KeLowerIrql(OldIrql);

        memset(Header + 1, 0, HeadSize);
        return Header + 1;
    }

    KeAcquireSpinLockAtCurrentIrql(&FreeBlockLock[Head]);
    if (FreeBlockList[Head].Next) {
        BlockHeader *Header =
            CONTAINING_RECORD(RtPopSList(&FreeBlockList[Head]), BlockHeader, ListHeader);

        if (Header->Head != Head) {
            KeFatalError(KE_PANIC_BAD_POOL_HEADER, (uint64_t)Header, Header->Head, Head, 0);
        }

        KeReleaseSpinLockAtCurrentIrql(&FreeBlockLock[Head]);
        memcpy(Header->Tag, Tag, 4);
        MiAddPoolTracker(FullSize, Tag);
        KeLowerIrql(OldIrql);

        memset(Header + 1, 0, HeadSize);
        return Header + 1;
    }
    KeReleaseSpinLockAtCurrentIrql(&FreeBlockLock[Head]);

    /* Allocate some extra space, and carve it into a bunch of Head-sized elements. */
    char *StartAddress = MiAllocatePoolPages(HeadPages);
    if (!StartAddress) {
        KeLowerIrql(OldIrql);
        return NULL;
    }

    /* Split the pages into equal sized chunks; This can have some waste depending on the chosen
     * bucket sizes, so make sure to tune the min/max/shift values! */
    KeAcquireSpinLockAtCurrentIrql(&FreeBlockLock[Head]);
    for (uint64_t i = 1; i < (HeadPages << MM_PAGE_SHIFT) / FullSize; i++) {
        BlockHeader *Header = (BlockHeader *)(StartAddress + i * FullSize);
        Header->Head = Head;
        RtPushSList(&FreeBlockList[Head], &Header->ListHeader);
    }

    KeReleaseSpinLockAtCurrentIrql(&FreeBlockLock[Head]);

    /* The first block was skipped as it should be ours. */
    BlockHeader *Header = (BlockHeader *)StartAddress;
    memcpy(Header->Tag, Tag, 4);
    MiAddPoolTracker(FullSize, Tag);
    KeLowerIrql(OldIrql);

    memset(Header + 1, 0, HeadSize);
    return Header + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the given block of memory to the free list.
 *
 * PARAMETERS:
 *     Base - Start of the block.
 *     Tag - Name/identifier attached to the block.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MmFreePool(void *Base, const char Tag[4]) {
    /* We should just crash right away if we're above DISPATCH. */
    KeIrql OldIrql = KeRaiseIrql(KE_IRQL_DISPATCH);

    /* MmAllocatePool guarantees anything that is inside the managed pool buckets is never going to
       be page aligned. */
    if (!((uint64_t)Base & (MM_PAGE_SIZE - 1))) {
        /* This should be mapped and have the tag properly setup, otherwise, we weren't allocated by
         * MmAllocatePool (maybe by MiAllocatePoolPages instead?). */
        uint64_t PhysicalAddress = HalpGetPhysicalAddress(Base);
        if (!PhysicalAddress) {
            KeFatalError(KE_PANIC_BAD_POOL_HEADER, (uint64_t)Base, 0, *(uint32_t *)Tag, UINT64_MAX);
        }

        MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);
        if (memcmp(Entry->Tag, Tag, 4)) {
            KeFatalError(
                KE_PANIC_BAD_POOL_HEADER,
                (uint64_t)Base,
                *(uint32_t *)Entry->Tag,
                *(uint32_t *)Tag,
                UINT64_MAX);
        }

        /* The remaining checks are directly done by MiFreePoolPages. */
        uint32_t Pages = MiFreePoolPages(Base);
        MiRemovePoolTracker(Pages << MM_PAGE_SHIFT, Tag);
        KeLowerIrql(OldIrql);
        return;
    }

    BlockHeader *Header = (BlockHeader *)Base - 1;
    if (memcmp(Header->Tag, Tag, 4) || Header->Head >= MM_POOL_BLOCK_COUNT ||
        Header->ListHeader.Next) {
        KeFatalError(
            KE_PANIC_BAD_POOL_HEADER,
            (uint64_t)Header,
            *(uint32_t *)Header->Tag,
            *(uint32_t *)Tag,
            Header->Head);
    }

    /* If we haven't overflow the local cache yet, just directly push to it (as it doesn't need any
     * locks). */
    uint64_t FullSize = GetHeadSize(Header->Head) + 16;
    KeProcessor *Processor = KeGetCurrentProcessor();
    if (Processor->FreePoolBlockListSize[Header->Head] < MI_PROCESSOR_POOL_CACHE_MAX_SIZE) {
        RtPushSList(&Processor->FreePoolBlockListHead[Header->Head], &Header->ListHeader);
        MiRemovePoolTracker(FullSize, Tag);
        Processor->FreePoolBlockListSize[Header->Head]++;
        KeLowerIrql(OldIrql);
        return;
    }

    /* TODO: At some point, we really should be returning this memory to the big pool allocator
     * (in case the whole segment is free). */
    KeAcquireSpinLockAtCurrentIrql(&FreeBlockLock[Header->Head]);
    RtPushSList(&FreeBlockList[Header->Head], &Header->ListHeader);
    KeReleaseSpinLockAtCurrentIrql(&FreeBlockLock[Header->Head]);
    MiRemovePoolTracker(FullSize, Tag);
    KeLowerIrql(OldIrql);
}
