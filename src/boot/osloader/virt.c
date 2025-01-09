/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <memory.h>
#include <stdlib.h>

static OslpMemoryArenaEntry OslpMemoryArenaEntries[ARENA_ENTRIES];
static OslpMemoryArenaEntry *OslpMemoryArena = NULL;
static int OslpMemoryArenaSize = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the virtual memory arena allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslpInitializeVirtualAllocator(void) {
    for (uint64_t i = 0; i < ARENA_ENTRIES; i++) {
        OslpMemoryArenaEntries[i].Base = ARENA_BASE + (i * ARENA_PAGE_SIZE);
        OslpMemoryArenaEntries[i].Next = i == ARENA_ENTRIES ? NULL : &OslpMemoryArenaEntries[i + 1];
    }

    OslpMemoryArena = OslpMemoryArenaEntries;
    OslpMemoryArenaSize = ARENA_ENTRIES;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a range of virtual addresses, randomizing the high bits if possible.
 *
 * PARAMETERS:
 *     Pages - Amount of pages, size of each one is given in memory.h.
 *
 * RETURN VALUE:
 *     Allocated address, or NULL if no address was found.
 *-----------------------------------------------------------------------------------------------*/
void *OslAllocateVirtualAddress(uint64_t Pages) {
    if (!Pages || !OslpMemoryArenaSize ||
        Pages * DEFAULT_PAGE_ALLOCATION_GRANULARITY > ARENA_PAGE_SIZE) {
        return NULL;
    }

    /* First stage, allocate one of the random areas; This will randomize at least a few of the
       high bits (on amd64, it randomizes 9 bits). We just generate a random index into the arena
       list. */
    unsigned int RandomIndex = (unsigned int)rand() % OslpMemoryArenaSize;
    OslpMemoryArenaEntry *Entry = OslpMemoryArena;
    uint64_t Address = 0;

    if (!RandomIndex) {
        OslpMemoryArena = OslpMemoryArena->Next;
        Address = Entry->Base;
    } else {
        while (--RandomIndex) {
            Entry = Entry->Next;
        }

        Address = Entry->Next->Base;
        Entry->Next = Entry->Next->Next;
    }

    /* Second stage, 10 attempts at randomizing the remaining bits. */
    for (int i = 0; i < 10; i++) {
        uint64_t RandomOffset =
            __rand64() & (ARENA_PAGE_SIZE - 1) & ~(DEFAULT_PAGE_ALLOCATION_GRANULARITY - 1);
        if (ARENA_PAGE_SIZE - RandomOffset >= Pages * DEFAULT_PAGE_ALLOCATION_GRANULARITY) {
            Address += RandomOffset;
            break;
        }
    }

    OslpMemoryArenaSize--;
    return (void *)Address;
}
