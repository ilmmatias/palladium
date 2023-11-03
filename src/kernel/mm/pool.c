/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <mi.h>
#include <rt.h>
#include <string.h>

#define SMALL_BLOCK_COUNT ((uint32_t)((MM_PAGE_SIZE - 16) >> 4))
#define HUGE_BLOCK_COUNT 256

typedef struct {
    RtSList ListHeader;
    char Tag[4];
    uint32_t Head;
} PoolHeader;

typedef struct {
    RtSList ListHeader;
    uint32_t Pages;
} PoolPagesHeader;

extern MiPageEntry *MiPageList;

RtSList SmallBlocks[SMALL_BLOCK_COUNT] = {};
RtSList HugeBlocks[HUGE_BLOCK_COUNT + 1] = {};

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
static void *AllocatePoolPages(uint32_t Pages) {
    uint32_t Head = Pages > HUGE_BLOCK_COUNT ? HUGE_BLOCK_COUNT : Pages - 1;

    /* Everything up to HUGE_BLOCK_COUNT are lists where each entry is an exact amount of pages,
       the entry past that represents anything over those limits. */
    for (uint32_t i = Head; i <= HUGE_BLOCK_COUNT; i++) {
        if (!HugeBlocks[i].Next ||
            CONTAINING_RECORD(HugeBlocks[i].Next, PoolPagesHeader, ListHeader)->Pages < Pages) {
            continue;
        }

        PoolPagesHeader *Header =
            CONTAINING_RECORD(RtPopSList(&HugeBlocks[i]), PoolPagesHeader, ListHeader);

        if (Header->Pages > Pages) {
            PoolPagesHeader *RemainingSpace =
                (PoolPagesHeader *)((char *)Header + (Pages << MM_PAGE_SHIFT));

            RemainingSpace->Pages = Header->Pages - Pages;

            RtPushSList(
                &HugeBlocks
                    [RemainingSpace->Pages > HUGE_BLOCK_COUNT ? HUGE_BLOCK_COUNT
                                                              : RemainingSpace->Pages - 1],
                &RemainingSpace->ListHeader);
        }

        /* Mark the start and end of the allocation (FreePoolPages depends on this). */
        uint64_t PhysicalAddress = MiGetPhysicalAddress(Header);
        MiPageList[PhysicalAddress >> 12].StartOfAllocation = 1;

        if (Pages != 1) {
            PhysicalAddress = MiGetPhysicalAddress((char *)Header + ((Pages - 1) << MM_PAGE_SHIFT));
        }

        MiPageList[PhysicalAddress >> 12].EndOfAllocation = 1;
        return Header;
    }

    /* Nothing left on the free list (that we can use), let's reach into the bitmap. */
    uint64_t Offset = RtFindClearBitsAndSet(&MiPoolBitmap, MiPoolBitmapHint, Pages);
    if (Offset == (uint64_t)-1) {
        return NULL;
    }

    MiPoolBitmapHint = Offset + Pages;

    char *VirtualAddress = (char *)MiPoolStart + (Offset << MM_PAGE_SHIFT);
    for (uint32_t i = 0; i < Pages; i++) {
        uint64_t PhysicalAddress = MmAllocatePages(1);
        if (!PhysicalAddress ||
            !MiMapPage(VirtualAddress + (i << MM_PAGE_SHIFT), PhysicalAddress, MI_MAP_WRITE)) {
            return NULL;
        }

        if (!i) {
            MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].StartOfAllocation = 1;
        }

        if (i == Pages - 1) {
            MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].EndOfAllocation = 1;
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
    uint64_t FirstPhysicalAddress = MiGetPhysicalAddress(Base);
    if (!MiPageList[FirstPhysicalAddress >> MM_PAGE_SHIFT].StartOfAllocation) {
        KeFatalError(KE_BAD_POOL_HEADER);
    }

    uint64_t Pages = 1;
    uint64_t LastPhysicalAddress = FirstPhysicalAddress;
    for (uint64_t i = 1; !MiPageList[LastPhysicalAddress >> MM_PAGE_SHIFT].EndOfAllocation; i++) {
        LastPhysicalAddress = MiGetPhysicalAddress((char *)Base + (i << MM_PAGE_SHIFT));
        Pages++;
    }

    PoolPagesHeader *Header = Base;
    Header->Pages = Pages;

    uint32_t Head = Pages > HUGE_BLOCK_COUNT ? HUGE_BLOCK_COUNT : Pages - 1;
    RtPushSList(&HugeBlocks[Head], &Header->ListHeader);

    MiPageList[FirstPhysicalAddress >> MM_PAGE_SHIFT].StartOfAllocation = 0;
    MiPageList[LastPhysicalAddress >> MM_PAGE_SHIFT].EndOfAllocation = 0;
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
    if (!Size) {
        Size = 1;
    }

    /* The header should always be 16 bytes, fix up the struct at the start of the file if the
       pointer size isn't 64-bits. */
    uint32_t Head = (Size + 0x0F) >> 4;
    if (Head > SMALL_BLOCK_COUNT) {
        return AllocatePoolPages((Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT);
    }

    /* Start at an exact match, and try everything onwards too (if there was nothing free). */
    for (uint32_t i = Head; i <= SMALL_BLOCK_COUNT; i++) {
        if (!SmallBlocks[i - 1].Next) {
            continue;
        }

        PoolHeader *Header =
            CONTAINING_RECORD(RtPopSList(&SmallBlocks[i - 1]), PoolHeader, ListHeader);

        if (Header->Head != i) {
            KeFatalError(KE_BAD_POOL_HEADER);
        }

        Header->Head = Head;
        memcpy(Header->Tag, Tag, 4);
        memset(Header + 1, 0, Head << 4);

        if (i - Head > 1) {
            PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
            RemainingSpace->Head = i - Head - 1;
            RtPushSList(&SmallBlocks[i - Head - 2], &RemainingSpace->ListHeader);
        }

        return Header + 1;
    }

    PoolHeader *Header = (PoolHeader *)AllocatePoolPages(1);
    if (!Header) {
        return NULL;
    }

    Header->ListHeader.Next = NULL;
    Header->Head = Head;
    memcpy(Header->Tag, Tag, 4);
    memset(Header + 1, 0, Head << 4);

    /* Wrap up by slicing the allocated page, we can add the remainder to the free list if
       it's big enough. */
    if (SMALL_BLOCK_COUNT - Head > 1) {
        PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
        RemainingSpace->Head = SMALL_BLOCK_COUNT - Head - 1;
        RtPushSList(&SmallBlocks[SMALL_BLOCK_COUNT - Head - 2], &RemainingSpace->ListHeader);
    }

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
    /* MmAllocatePool guarantees anything that is inside the small pool buckets is never going to
       be page aligned. */
    if (!((uint64_t)Base & (MM_PAGE_SIZE - 1))) {
        FreePoolPages(Base);
        return;
    }

    PoolHeader *Header = (PoolHeader *)Base - 1;

    if (memcmp(Header->Tag, Tag, 4) || Header->Head < 1 || Header->Head >= SMALL_BLOCK_COUNT) {
        KeFatalError(KE_BAD_POOL_HEADER);
    }

    if (Header->ListHeader.Next) {
        KeFatalError(KE_DOUBLE_POOL_FREE);
    }

    RtPushSList(&SmallBlocks[Header->Head - 1], &Header->ListHeader);
}