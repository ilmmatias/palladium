/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <memory.h>
#include <stdlib.h>

MemoryArena *BiMemoryArena = NULL;
int BiMemoryArenaSize = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a range of virtual addresses, randomizing the high bits if possible.
 *
 * PARAMETERS:
 *     Pages - Amount of pages, size of each one is given in memory.h.
 *
 * RETURN VALUE:
 *     Allocated address, or 0 if no address was found.
 *-----------------------------------------------------------------------------------------------*/
uint64_t BmAllocateVirtualAddress(uint64_t Pages) {
    if (!Pages || !BiMemoryArenaSize || (Pages << PAGE_SHIFT) > ARENA_PAGE_SIZE) {
        return 0;
    }

    /* First stage, allocate one of the random areas; This will randomize at least a few of the
       high bits (on amd64, it randomizes 9 bits). We just generate a random index into the arena
       list. */
    unsigned int RandomIndex = (unsigned int)rand() % BiMemoryArenaSize;
    MemoryArena *Entry = BiMemoryArena;
    uint64_t Address = 0;

    if (!RandomIndex) {
        BiMemoryArena = BiMemoryArena->Next;
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
        uint64_t RandomOffset = __rand64() & (ARENA_PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
        if (ARENA_PAGE_SIZE - RandomOffset >= (Pages << PAGE_SHIFT)) {
            Address += RandomOffset;
            break;
        }
    }

    BiMemoryArenaSize--;
    return Address;
}
