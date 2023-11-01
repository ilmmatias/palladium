/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <mm.h>
#include <rt.h>
#include <string.h>

typedef struct {
    RtSinglyLinkedListEntry ListHeader;
    char Tag[4];
    uint32_t Head;
} PoolHeader;

#define BLOCK_COUNT ((uint32_t)((MM_PAGE_SIZE - 16) >> 4))
RtSinglyLinkedListEntry Blocks[BLOCK_COUNT] = {};

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
    if (Head > BLOCK_COUNT) {
        return NULL;
    }

    if (Blocks[Head - 1].Next) {
        PoolHeader *Header =
            CONTAINING_RECORD(RtPopSinglyLinkedList(&Blocks[Head - 1]), PoolHeader, ListHeader);

        if (Header->Head != Head) {
            KeFatalError(KE_BAD_POOL_HEADER);
        }

        memcpy(Header->Tag, Tag, 4);
        memset(Header + 1, 0, Head << 4);

        return Header + 1;
    }

    /* Before possibly wasting over half a page (for lots of small pool allocations), let's see
       if we can split some larger page. */
    for (uint32_t i = Head + 1; i < BLOCK_COUNT; i++) {
        if (!Blocks[i - 1].Next) {
            continue;
        }

        PoolHeader *Header =
            CONTAINING_RECORD(RtPopSinglyLinkedList(&Blocks[i - 1]), PoolHeader, ListHeader);

        if (Header->Head != i) {
            KeFatalError(KE_BAD_POOL_HEADER);
        }

        Header->Head = Head;
        memcpy(Header->Tag, Tag, 4);
        memset(Header + 1, 0, Head << 4);

        if (i - Head > 1) {
            PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
            RemainingSpace->Head = i - Head - 1;
            RtPushSinglyLinkedList(&Blocks[i - Head - 2], &RemainingSpace->ListHeader);
        }

        return Header + 1;
    }

    PoolHeader *Header = (PoolHeader *)MmAllocatePages(1);
    if (!Header) {
        return NULL;
    }

    Header = (PoolHeader *)MI_PADDR_TO_VADDR((uint64_t)Header);
    memset(Header, 0, sizeof(PoolHeader));

    Header->Head = Head;
    memcpy(Header->Tag, Tag, 4);
    memset(Header + 1, 0, Head << 4);

    /* Wrap up by slicing the allocated page, we can add the remainder to the free list if
       it's big enough. */
    if (BLOCK_COUNT - Head > 1) {
        PoolHeader *RemainingSpace = (PoolHeader *)((char *)Header + (Head << 4) + 16);
        RemainingSpace->Head = BLOCK_COUNT - Head - 1;
        RtPushSinglyLinkedList(&Blocks[BLOCK_COUNT - Head - 2], &RemainingSpace->ListHeader);
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
    PoolHeader *Header = (PoolHeader *)Base - 1;

    if (memcmp(Header->Tag, Tag, 4) || Header->Head < 1 || Header->Head >= BLOCK_COUNT) {
        KeFatalError(KE_BAD_POOL_HEADER);
    }

    if (Header->ListHeader.Next) {
        KeFatalError(KE_DOUBLE_POOL_FREE);
    }

    RtPushSinglyLinkedList(&Blocks[Header->Head - 1], &Header->ListHeader);
}
