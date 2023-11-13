/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <memory.h>
#include <string.h>

BiMemoryDescriptor BiMemoryDescriptors[BI_MAX_MEMORY_DESCRIPTORS] = {};
int BiMemoryDescriptorCount = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a new memory descriptor to the physical page map.
 *
 * PARAMETERS:
 *     Type - Type of the descriptor.
 *     Base - First address of the range.
 *     Size - How many bytes the range takes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiAddMemoryDescriptor(int Type, uint64_t Base, uint64_t Size) {
    /* Skip into the first index we could insert ourselves (if we don't overlap). */
    int Index = 0;
    while (Index < BiMemoryDescriptorCount && BiMemoryDescriptors[Index].Base <= Base) {
        Index++;
    }

    /* The code below covers a bunch of common patterns, but it's still missing things, so there'll
       probably be memory map holes on anything too complex. */

    /* If we overlap in either sides, we should be less (or as) restrictive as whatever we're
       overlapping, if we're not, bail out. */
    int OverlapsLeft =
        Index && Base < BiMemoryDescriptors[Index - 1].Base + BiMemoryDescriptors[Index - 1].Size;
    int OverlapsRight =
        Index < BiMemoryDescriptorCount && Base + Size > BiMemoryDescriptors[Index].Base;

    if ((OverlapsLeft && Type > BiMemoryDescriptors[Index - 1].Type) ||
        (OverlapsRight && Type > BiMemoryDescriptors[Index].Type)) {
        BmPrint(
            "The system's memory map is in an inconsistent state, and was deemed unsafe to use.\n"
            "You'll need to restart your device.\n");
        while (1)
            ;
    }

    /* | type 1 |       */
    /*     |  type 0  | */
    /* this becomes: */
    /* | type 1 |       */
    /*          |  t0 | */
    if (OverlapsLeft && Type != BiMemoryDescriptors[Index - 1].Type) {
        uint64_t NewBase =
            BiMemoryDescriptors[Index - 1].Base + BiMemoryDescriptors[Index - 1].Size;
        Size = Base + Size - NewBase;
        Base = NewBase;
        OverlapsLeft = 0;
    } else {
        OverlapsLeft |=
            Index && Type == BiMemoryDescriptors[Index - 1].Type &&
            Base == BiMemoryDescriptors[Index - 1].Base + BiMemoryDescriptors[Index - 1].Size;
    }

    /*       | type 1 | */
    /* |  type 0  | */
    /* this becomes: */
    /*       | type 1 | */
    /* |  t0 |          */
    if (OverlapsRight && Type != BiMemoryDescriptors[Index].Type) {
        Size = BiMemoryDescriptors[Index].Base - Base;
        OverlapsRight = 0;
    } else {
        OverlapsRight |= Index < BiMemoryDescriptorCount &&
                         Type == BiMemoryDescriptors[Index].Type &&
                         Base + Size == BiMemoryDescriptors[Index].Base;
    }

    /* | left entry |     | right entry | */
    /*       |        us         |        */
    if (OverlapsLeft && OverlapsRight) {
        BiMemoryDescriptors[Index - 1].Size = BiMemoryDescriptors[Index].Base +
                                              BiMemoryDescriptors[Index].Size -
                                              BiMemoryDescriptors[Index - 1].Base;

        memmove(
            &BiMemoryDescriptors[Index],
            &BiMemoryDescriptors[Index + 1],
            (BiMemoryDescriptorCount - Index - 1) * sizeof(BiMemoryDescriptor));
        BiMemoryDescriptorCount--;

        return;
    }

    /* | left entry |    */
    /*    |     us     | */
    if (OverlapsLeft) {
        BiMemoryDescriptors[Index - 1].Size = Base + Size - BiMemoryDescriptors[Index - 1].Base;
        return;
    }

    /*     | right entry | */
    /* |     us     |      */
    if (OverlapsRight) {
        BiMemoryDescriptors[Index].Size =
            BiMemoryDescriptors[Index].Base + BiMemoryDescriptors[Index].Size - Base;
        BiMemoryDescriptors[Index].Base = Base;
        return;
    }

    /* For non overlapping entries, move all entries forward (if we're not the last entry),
       write up the entry, and we're done. */
    if (BiMemoryDescriptorCount >= BI_MAX_MEMORY_DESCRIPTORS) {
        return;
    } else if (Index < BiMemoryDescriptorCount) {
        memmove(
            &BiMemoryDescriptors[Index + 1],
            &BiMemoryDescriptors[Index],
            (BiMemoryDescriptorCount - Index) * sizeof(BiMemoryDescriptor));
    }

    BiMemoryDescriptors[Index].Type = Type;
    BiMemoryDescriptors[Index].Base = Base;
    BiMemoryDescriptors[Index].Size = Size;
    BiMemoryDescriptorCount++;
}
