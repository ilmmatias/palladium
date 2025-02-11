/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/ke.h>
#include <kernel/mi.h>
#include <string.h>

#define SMALL_BLOCK_COUNT ((uint32_t)((MM_PAGE_SIZE - 16) >> 4))

typedef struct {
    RtSList ListHeader;
    char Tag[4];
    uint32_t Head;
} SmallBlockHeader;

static KeSpinLock Lock = {0};
static RtSList SmallBlocks[SMALL_BLOCK_COUNT] = {};

char *MiPoolVirtualOffset = NULL;
RtDList MiPoolBigFreeListHead;
RtDList MiPoolFreeTagListHead;
RtDList MiPoolTagListHead;

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
    MiPoolVirtualOffset = (char *)MI_POOL_START;
    RtInitializeDList(&MiPoolBigFreeListHead);
    RtInitializeDList(&MiPoolTagListHead);
    RtInitializeDList(&MiPoolFreeTagListHead);
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
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Lock, KE_IRQL_DISPATCH);

    if (!Size) {
        Size = 1;
    }

    /* The header should always be 16 bytes, fix up the struct at the start of the file if the
       pointer size isn't 64-bits. */
    uint32_t Head = (Size + 0x0F) >> 4;
    if (Head > SMALL_BLOCK_COUNT) {
        uint32_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
        void *Base = MiAllocatePoolPages(Pages);

        /* Tag the allocation and try to account for it in the pool tracker. */
        memcpy(MI_PAGE_ENTRY(HalpGetPhysicalAddress(Base)).Tag, Tag, 4);
        MiAddPoolTracker(Pages << MM_PAGE_SHIFT, Tag);

        /* We don't need locking from here on out (we'd just be wasting time). */
        KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);

        if (Base) {
            memset(Base, 0, Pages << MM_PAGE_SHIFT);
        }

        return Base;
    }

    /* Start at an exact match, and try everything onwards too (if there was nothing free). */
    for (uint32_t i = Head; i <= SMALL_BLOCK_COUNT; i++) {
        if (!SmallBlocks[i - 1].Next) {
            continue;
        }

        SmallBlockHeader *Header =
            CONTAINING_RECORD(RtPopSList(&SmallBlocks[i - 1]), SmallBlockHeader, ListHeader);

        if (Header->Head != i) {
            KeFatalError(
                KE_PANIC_BAD_POOL_HEADER,
                (uint64_t)Header,
                *(uint32_t *)Header->Tag,
                *(uint32_t *)Tag,
                Header->Head);
        }

        Header->Head = Head;
        memcpy(Header->Tag, Tag, 4);

        if (i - Head > 1) {
            SmallBlockHeader *RemainingSpace =
                (SmallBlockHeader *)((char *)Header + (Head << 4) + 16);
            RemainingSpace->Head = i - Head - 1;
            RtPushSList(&SmallBlocks[i - Head - 2], &RemainingSpace->ListHeader);
        }

        /* We don't need locking from here on out (we'd just be wasting time). */
        MiAddPoolTracker(Head << 4, Tag);
        KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
        memset(Header + 1, 0, Head << 4);
        return Header + 1;
    }

    SmallBlockHeader *Header = MiAllocatePoolPages(1);
    if (!Header) {
        KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
        return NULL;
    }

    Header->ListHeader.Next = NULL;
    Header->Head = Head;
    memcpy(Header->Tag, Tag, 4);

    /* Wrap up by slicing the allocated page, we can add the remainder to the free list if
       it's big enough. */
    if (SMALL_BLOCK_COUNT - Head > 1) {
        SmallBlockHeader *RemainingSpace = (SmallBlockHeader *)((char *)Header + (Head << 4) + 16);
        RemainingSpace->Head = SMALL_BLOCK_COUNT - Head - 1;
        RtPushSList(&SmallBlocks[SMALL_BLOCK_COUNT - Head - 2], &RemainingSpace->ListHeader);
    }

    /* memset() inside the spin lock would be wasting time. */
    MiAddPoolTracker(Head << 4, Tag);
    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
    memset(Header + 1, 0, Head << 4);
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
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Lock, KE_IRQL_DISPATCH);

    /* MmAllocatePool guarantees anything that is inside the small pool buckets is never going to
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
        KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
        return;
    }

    SmallBlockHeader *Header = (SmallBlockHeader *)Base - 1;
    if (memcmp(Header->Tag, Tag, 4) || Header->Head < 1 || Header->Head >= SMALL_BLOCK_COUNT ||
        Header->ListHeader.Next) {
        KeFatalError(
            KE_PANIC_BAD_POOL_HEADER,
            (uint64_t)Header,
            *(uint32_t *)Header->Tag,
            *(uint32_t *)Tag,
            Header->Head);
    }

    /* TODO: Reduce fragmentation. */
    MiRemovePoolTracker(Header->Head << 4, Tag);
    RtPushSList(&SmallBlocks[Header->Head - 1], &Header->ListHeader);
    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
}
