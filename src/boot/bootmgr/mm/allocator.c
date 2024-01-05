/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <memory.h>
#include <string.h>

typedef struct AllocatorEntry {
    int Used;
    size_t Size;
    struct AllocatorEntry *Prev, *Next;
} AllocatorEntry;

static AllocatorEntry *AllocatorHead = NULL, *AllocatorTail = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function splits a block of memory into two blocks, one of the required size for the
 *     allocation and one of the remaining size.
 *
 * PARAMETERS:
 *     Entry - The entry to split.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void SplitEntry(AllocatorEntry *Entry, size_t Size) {
    if (Entry->Size <= Size + sizeof(AllocatorEntry)) {
        return;
    }

    AllocatorEntry *NewEntry = (AllocatorEntry *)((uintptr_t)Entry + sizeof(AllocatorEntry) + Size);

    NewEntry->Used = 0;
    NewEntry->Size = Entry->Size - (Size + sizeof(AllocatorEntry));
    NewEntry->Prev = Entry;
    NewEntry->Next = NULL;

    AllocatorTail = NewEntry;

    Entry->Size = Size;
    Entry->Next = NewEntry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function merges all contiguous free entries after and including the base entry into
 *     one entry.
 *
 * PARAMETERS:
 *     Base - The entry to start merging from.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void MergeEntriesForward(AllocatorEntry *Base) {
    while (Base->Next && Base + (Base->Size << 12) == Base->Next && !Base->Next->Used) {
        Base->Size += Base->Next->Size;
        Base->Next = Base->Next->Next;

        if (Base->Next) {
            Base->Next->Prev = Base;
        }
    }

    if (!Base->Next) {
        AllocatorTail = Base;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function merges all contiguous free entries before and including the base entry into
 *     one entry.
 *
 * PARAMETERS:
 *     Base - The entry to start merging from.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void MergeEntriesBackward(AllocatorEntry *Base) {
    while (Base->Prev && Base->Prev + (Base->Prev->Size << 12) == Base && !Base->Prev->Used) {
        Base->Prev->Size += Base->Size;
        Base->Prev->Next = Base->Next;

        if (Base->Next) {
            Base->Next->Prev = Base->Prev;
        }

        Base = Base->Prev;
    }

    if (!Base->Prev) {
        AllocatorHead = Base;
    }

    if (!Base->Next) {
        AllocatorTail = Base;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finds a free entry of the specified size, or creates requests a page
 *     and uses it to create a new entry.
 *
 * PARAMETERS:
 *     Size - The size of the entry to find or create.
 *
 * RETURN VALUE:
 *     A pointer to the allocated entry, or NULL if there is no more free regions in the memory
 *     map.
 *-----------------------------------------------------------------------------------------------*/
static AllocatorEntry *FindFreeEntry(size_t Size) {
    AllocatorEntry *Entry = AllocatorHead;
    size_t Mask = BI_PAGE_SIZE - 1;

    while (Entry) {
        if (!Entry->Used && Entry->Size >= Size) {
            Entry->Used = 1;
            return Entry;
        }

        Entry = Entry->Next;
    }

    Entry = BmAllocatePages(Size, BM_MD_BOOTMGR);
    Size = ((Size + sizeof(AllocatorEntry) + Mask) & ~Mask) - sizeof(AllocatorEntry);

    if (!Entry) {
        return NULL;
    }

    Entry->Used = 1;
    Entry->Size = Size;
    Entry->Prev = AllocatorTail;
    Entry->Next = NULL;

    if (AllocatorTail) {
        AllocatorTail->Next = Entry;
    } else {
        AllocatorHead = Entry;
    }

    AllocatorTail = Entry;

    return Entry;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a block of memory of the specified size.
 *
 * PARAMETERS:
 *     Size - The size of the block to allocate.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is was no free entry and requesting
 *     a new page failed.
 *-----------------------------------------------------------------------------------------------*/
void *BmAllocateBlock(size_t Size) {
    Size = (Size + 15) & ~0x0F;
    AllocatorEntry *Entry = FindFreeEntry(Size);

    if (Entry) {
        SplitEntry(Entry, Size);
        return Entry + 1;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a block of memory for an array of `num` elements, zeroing it by
 *     afterwards.
 *
 * PARAMETERS:
 *     Elements - How many elements the array has.
 *     ElementSize - The size of each element.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is was no free entry and requesting
 *     a new page failed.
 *-----------------------------------------------------------------------------------------------*/
void *BmAllocateZeroBlock(size_t Elements, size_t ElementSize) {
    size_t Size = Elements * ElementSize;
    void *Base = BmAllocateBlock(Size);

    if (Base) {
        memset(Base, 0, Size);
    }

    return Base;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees a block of memory.
 *
 * PARAMETERS:
 *     Block - The base address of the block to free.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmFreeBlock(void *Block) {
    AllocatorEntry *Entry = (AllocatorEntry *)Block - 1;
    Entry->Used = 0;
    MergeEntriesForward(Entry);
    MergeEntriesBackward(Entry);
}
