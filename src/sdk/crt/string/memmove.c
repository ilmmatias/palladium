/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to safely copy bytes from one region of memory to
 *     another, handling any overlap.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     count - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memmove(void *dest, const void *src, size_t count) {
    char *Destination = (char *)dest;
    const char *Source = (const char *)src;

    /*  Overlapping like this:
        <src----->
            <dest--->
        requires a backwards copy, or else we'll overwrite data before we copy it.
        Non overlapping can use just a memcpy, and overlapping like this NEEDS to use it:
        <dest--->
           <src---->
        so we just call memcpy for both. */
    if (Source >= Destination || Source + count <= Destination) {
        return memcpy(dest, src, count);
    }

    Destination += count;
    Source += count;

    while (count--) {
        *(--Destination) = *(--Source);
    }

    return dest;
}
