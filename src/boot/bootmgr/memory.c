/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

MemoryArena *BmMemoryArena = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates a range of virtual addresses, randomizing the high bits if possible.
 *
 * PARAMETERS:
 *     LargePages - Amount of large pages, size of each one is given in memory.h.
 *
 * RETURN VALUE:
 *     Allocated address, or 0 if no address was found.
 *-----------------------------------------------------------------------------------------------*/
uint64_t BmAllocateVirtualAddress(uint64_t LargePages) {
    if (!LargePages) {
        return 0;
    }

    /* Generate a random sequence, fit it inside the arena size, and then strip the low page
       bits. */
    uint64_t RandomBits = __rand64();
    uint64_t Address = ARENA_BASE + (RandomBits & ((ARENA_SIZE - 1) & ~(LARGE_PAGE_SIZE - 1)));
    uint64_t Size = LargePages << LARGE_PAGE_SHIFT;

    MemoryArena *Entry = BmMemoryArena;
    MemoryArena *Closest = NULL;
    int ExactMatch = 0;

    while (Entry != NULL) {
        if (Entry->Base <= Address && Entry->Base + Entry->Size >= Address + Size) {
            ExactMatch = 1;
            break;
        }

        /* If enough memory is available, save the address and closest match and keep on searching
           until we trip to the first entry after the chosen address. */
        if (Entry->Size >= Size) {
            Closest = Entry;

            if (Entry->Base + Entry->Size >= Address) {
                break;
            }
        }

        Entry = Entry->Next;
    }

    if (!ExactMatch && !Closest) {
        return 0;
    } else if (!ExactMatch || Entry->Base == Address) {
        Entry->Size -= Size;
        Address = Entry->Base;
        Entry->Base += Size;
        return Address;
    }

    MemoryArena *NextEntry = malloc(sizeof(MemoryArena));
    if (!NextEntry) {
        return 0;
    }

    NextEntry->Base = Address + Size;
    NextEntry->Size = Entry->Size - NextEntry->Base + Entry->Base;
    NextEntry->Next = Entry->Next;

    Entry->Size = Address - Entry->Base;
    Entry->Next = NextEntry;

    return Address;
}
