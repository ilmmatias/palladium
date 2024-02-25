/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
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

RtSList SmallBlocks[SMALL_BLOCK_COUNT] = {};

uint64_t MiPoolStart = 0;
uint64_t MiPoolBitmapHint = 0;
RtBitmap MiPoolBitmap;

static KeSpinLock Lock = {0};

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
    uint64_t Offset = RtFindClearBitsAndSet(&MiPoolBitmap, MiPoolBitmapHint, Pages);
    if (Offset == (uint64_t)-1) {
        return NULL;
    }

    MiPoolBitmapHint = Offset + Pages;

    char *VirtualAddress = (char *)MiPoolStart + (Offset << MM_PAGE_SHIFT);
    for (uint32_t i = 0; i < Pages; i++) {
        uint64_t PhysicalAddress = MmAllocatePage();
        if (!PhysicalAddress ||
            !HalpMapPage(VirtualAddress + (i << MM_PAGE_SHIFT), PhysicalAddress, MI_MAP_WRITE)) {
            return NULL;
        }

        if (!i) {
            MiPageList[PhysicalAddress >> MM_PAGE_SHIFT].Pages = Pages;
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
    uint64_t FirstPhysicalAddress = HalpGetPhysicalAddress(Base);
    uint32_t Pages = MiPageList[FirstPhysicalAddress >> MM_PAGE_SHIFT].Pages;

    for (uint32_t i = 0; i < Pages; i++) {
        MmDereferencePage(HalpGetPhysicalAddress((char *)Base + (i << MM_PAGE_SHIFT)));
    }

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
    if (!Size) {
        Size = 1;
    }

    /* The header should always be 16 bytes, fix up the struct at the start of the file if the
       pointer size isn't 64-bits. */
    uint32_t Head = (Size + 0x0F) >> 4;
    if (Head > SMALL_BLOCK_COUNT) {
        uint32_t Pages = (Size + MM_PAGE_SIZE - 1) >> MM_PAGE_SHIFT;
        void *Base = AllocatePoolPages(Pages);

        if (Base) {
            memset(Base, 0, Pages << MM_PAGE_SHIFT);
        }

        return Base;
    }

    KeIrql Irql = KeAcquireSpinLock(&Lock);

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

        KeReleaseSpinLock(&Lock, Irql);

        Header->Head = Head;
        memcpy(Header->Tag, Tag, 4);

        if (i - Head > 1) {
            PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
            RemainingSpace->Head = i - Head - 1;
            RtPushSList(&SmallBlocks[i - Head - 2], &RemainingSpace->ListHeader);
        }

        memset(Header + 1, 0, Head << 4);
        return Header + 1;
    }

    PoolHeader *Header = (PoolHeader *)AllocatePoolPages(1);
    if (!Header) {
        KeReleaseSpinLock(&Lock, Irql);
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

    KeReleaseSpinLock(&Lock, Irql);
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

    KeIrql Irql = KeAcquireSpinLock(&Lock);
    RtPushSList(&SmallBlocks[Header->Head - 1], &Header->ListHeader);
    KeReleaseSpinLock(&Lock, Irql);
}
