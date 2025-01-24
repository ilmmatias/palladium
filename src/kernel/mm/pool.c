/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mi.h>
#include <rt/bitmap.h>
#include <string.h>

#define SMALL_BLOCK_COUNT ((uint32_t)((MM_PAGE_SIZE - 16) >> 4))

typedef struct {
    RtSList ListHeader;
    char Tag[4];
    uint32_t Head;
} PoolHeader;

extern MiPageEntry *MiPageList;
extern RtDList MiFreePageListHead;
extern KeSpinLock MiPageListLock;

static KeSpinLock Lock = {0};
static RtSList SmallBlocks[SMALL_BLOCK_COUNT] = {};

uint64_t MiPoolStart = 0;
uint64_t MiPoolBitmapHint = 0;
RtBitmap MiPoolBitmap;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a the specified amount of pages from the pool space.
 *
 * PARAMETERS:
 *     Pages - How many pages we need.
 *
 * RETURN VALUE:
 *     Virtual (mapped) pointer to the allocated space, or NULL if we failed to allocate it.
 *-----------------------------------------------------------------------------------------------*/
static void *AllocatePoolPages(uint64_t Pages) {
    uint64_t Offset = RtFindClearBitsAndSet(&MiPoolBitmap, MiPoolBitmapHint, Pages);
    if (Offset == (uint64_t)-1) {
        return NULL;
    }

    MiPoolBitmapHint = Offset + Pages;

    char *VirtualAddress = (char *)MiPoolStart + (Offset << MM_PAGE_SHIFT);
    for (uint64_t i = 0; i < Pages; i++) {
        uint64_t PhysicalAddress = MmAllocateSinglePage();
        if (!PhysicalAddress ||
            !HalpMapPage(VirtualAddress + (i << MM_PAGE_SHIFT), PhysicalAddress, MI_MAP_WRITE)) {
            return NULL;
        }

        MiPageEntry *Entry = &MI_PAGE_ENTRY(PhysicalAddress);
        if (i) {
            Entry->Flags |= MI_PAGE_FLAGS_POOL_ITEM;
        } else {
            Entry->Flags |= MI_PAGE_FLAGS_POOL_BASE;
            Entry->Pages = Pages;
        }
    }

    return VirtualAddress;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns all pages belonging to the given allocation into the free list.
 *
 * PARAMETERS:
 *     Base - First virtual address of the allocation.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void FreePoolPages(void *Base) {
    KeIrql OldIrql = KeAcquireSpinLock(&MiPageListLock);
    MiPageEntry *BaseEntry = &MI_PAGE_ENTRY(HalpGetPhysicalAddress(Base));
    if (!(BaseEntry->Flags & MI_PAGE_FLAGS_USED) || !(BaseEntry->Flags & MI_PAGE_FLAGS_POOL_BASE)) {
        KeFatalError(KE_PANIC_BAD_PFN_HEADER);
    }

    uint32_t Pages = BaseEntry->Pages;
    BaseEntry->Flags = 0;
    RtPushDList(&MiFreePageListHead, &BaseEntry->ListHeader);

    for (uint32_t Offset = MM_PAGE_SIZE; Offset < Pages << MM_PAGE_SHIFT; Offset += MM_PAGE_SIZE) {
        MiPageEntry *ItemEntry = &MI_PAGE_ENTRY(HalpGetPhysicalAddress((char *)Base + Offset));
        if (!(ItemEntry->Flags & MI_PAGE_FLAGS_USED) ||
            !(ItemEntry->Flags & MI_PAGE_FLAGS_POOL_ITEM)) {
            KeFatalError(KE_PANIC_BAD_PFN_HEADER);
        }

        ItemEntry->Flags = 0;
        RtPushDList(&MiFreePageListHead, &ItemEntry->ListHeader);
    }

    KeReleaseSpinLock(&MiPageListLock, OldIrql);
    RtClearBits(&MiPoolBitmap, ((uint64_t)Base - MiPoolStart) >> MM_PAGE_SHIFT, Pages);
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
    KeIrql OldIrql = KeAcquireSpinLock(&Lock);

    if (!Size) {
        Size = 1;
    }

    /* The header should always be 16 bytes, fix up the struct at the start of the file if the
       pointer size isn't 64-bits. */
    uint32_t Head = (Size + 0x0F) >> 4;
    if (Head > SMALL_BLOCK_COUNT) {
        uint64_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
        void *Base = AllocatePoolPages(Pages);

        /* We don't need locking from here on out (we'd just be wasting time). */
        KeReleaseSpinLock(&Lock, OldIrql);

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

        PoolHeader *Header =
            CONTAINING_RECORD(RtPopSList(&SmallBlocks[i - 1]), PoolHeader, ListHeader);

        if (Header->Head != i) {
            KeFatalError(KE_PANIC_BAD_POOL_HEADER);
        }

        Header->Head = Head;
        memcpy(Header->Tag, Tag, 4);

        if (i - Head > 1) {
            PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
            RemainingSpace->Head = i - Head - 1;
            RtPushSList(&SmallBlocks[i - Head - 2], &RemainingSpace->ListHeader);
        }

        /* We don't need locking from here on out (we'd just be wasting time). */
        KeReleaseSpinLock(&Lock, OldIrql);

        memset(Header + 1, 0, Head << 4);
        return Header + 1;
    }

    PoolHeader *Header = (PoolHeader *)AllocatePoolPages(1);
    if (!Header) {
        KeReleaseSpinLock(&Lock, OldIrql);
        return NULL;
    }

    Header->ListHeader.Next = NULL;
    Header->Head = Head;
    memcpy(Header->Tag, Tag, 4);

    /* Wrap up by slicing the allocated page, we can add the remainder to the free list if
       it's big enough. */
    if (SMALL_BLOCK_COUNT - Head > 1) {
        PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
        RemainingSpace->Head = SMALL_BLOCK_COUNT - Head - 1;
        RtPushSList(&SmallBlocks[SMALL_BLOCK_COUNT - Head - 2], &RemainingSpace->ListHeader);
    }

    /* memset() inside the spin lock would be wasting time. */
    KeReleaseSpinLock(&Lock, OldIrql);

    memset(Header + 1, 0, Head << 4);
    return Header + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the given block of memory to the free list.
 *
 * PARAMETERS:
 *     Base - Start of the block.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void MmFreePool(void *Base, const char Tag[4]) {
    KeIrql OldIrql = KeAcquireSpinLock(&Lock);

    /* MmAllocatePool guarantees anything that is inside the small pool buckets is never going to
       be page aligned. */
    if (!((uint64_t)Base & (MM_PAGE_SIZE - 1))) {
        FreePoolPages(Base);
        KeReleaseSpinLock(&Lock, OldIrql);
        return;
    }

    PoolHeader *Header = (PoolHeader *)Base - 1;

    if (memcmp(Header->Tag, Tag, 4) || Header->Head < 1 || Header->Head >= SMALL_BLOCK_COUNT ||
        Header->ListHeader.Next) {
        KeFatalError(KE_PANIC_BAD_POOL_HEADER);
    }

    RtPushSList(&SmallBlocks[Header->Head - 1], &Header->ListHeader);
    KeReleaseSpinLock(&Lock, OldIrql);
}
