/* SPDX-FileCopyrightText: (C) 2023 yuuma03
 * SPDX-License-Identifier: BSD-3-Clause */

#include <bios.h>
#include <boot.h>

typedef struct AllocatorEntry {
    int Used;
    size_t Size;
    struct AllocatorEntry *Prev, *Next;
} AllocatorEntry;

static BiosMemoryRegion *BiosMemoryMap = NULL;
static uint32_t BiosMemoryMapEntries = 0;
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
 *     This function finds a free entry of the specified size, or creates a new one using the
 *     memory map.
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

    while (Entry) {
        if (!Entry->Used && Entry->Size >= Size) {
            Entry->Used = 1;
            return Entry;
        }

        Entry = Entry->Next;
    }

    uint32_t Pages = (Size + sizeof(AllocatorEntry) + 0xFFF) >> 12;
    Size = ((Size + sizeof(AllocatorEntry) + 0xFFF) & ~0xFFF) - sizeof(AllocatorEntry);

    for (uint32_t i = 0; i < BiosMemoryMapEntries; i++) {
        BiosMemoryRegion *Region = &BiosMemoryMap[i];

        if ((Region->Type == BIOS_MEMORY_REGION_TYPE_AVAILABLE ||
                Region->Type == BIOS_MEMORY_REGION_TYPE_BOOTMANAGER) &&
            Region->Length >= (Region->UsedPages + Pages) << 12) {
            AllocatorEntry *Entry =
                (AllocatorEntry *)(Region->BaseAddress + (Region->UsedPages << 12));

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
            Region->Type = BIOS_MEMORY_REGION_TYPE_BOOTMANAGER;
            Region->UsedPages += Pages;

            return Entry;
        }
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up the memory allocator for the boot manager.
 *
 * PARAMETERS:
 *     BootBlock - Information obtained while setting up the boot environment.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmInitMemory(void *BootBlock) {
    BiosBootBlock *Data = (BiosBootBlock *)BootBlock;

    BiosMemoryMap = (BiosMemoryRegion *)(uintptr_t)Data->MemoryRegions;
    BiosMemoryMapEntries = Data->MemoryCount;

    for (uint32_t i = 0; i < BiosMemoryMapEntries; i++) {
        BiosMemoryRegion *Region = &BiosMemoryMap[i];

        if (Region->Type == BIOS_MEMORY_REGION_TYPE_AVAILABLE) {
            if (Region->BaseAddress == 0) {
                Region->UsedPages = 16;
            } else {
                Region->UsedPages = 0;
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a block of memory of the specified size.
 *
 * PARAMETERS:
 *     Size - The size of the block to allocate.
 *
 * RETURN VALUE:
 *     A pointer to the allocated block, or NULL if there is no more free regions in the memory
 *     map.
 *-----------------------------------------------------------------------------------------------*/
void *BmAllocate(size_t Size) {
    AllocatorEntry *Entry = FindFreeEntry(Size);

    if (Entry) {
        SplitEntry(Entry, Size);
        return Entry + 1;
    }

    return NULL;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function frees a block of memory.
 *
 * PARAMETERS:
 *     Base - The base address of the block to free.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmFree(void *Base) {
    AllocatorEntry *Entry = (AllocatorEntry *)Base - 1;
    Entry->Used = 0;
    MergeEntriesForward(Entry);
    MergeEntriesBackward(Entry);
}
