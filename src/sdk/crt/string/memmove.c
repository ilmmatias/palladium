/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to safely copy bytes from one region of memory to
 *     another, handling any overlap.
 *
 * PARAMETERS:
 *     s1 - Destination buffer.
 *     s2 - Source buffer.
 *     n - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memmove(void *s1, const void *s2, size_t n) {
    char *Destination = (char *)s1;
    const char *Source = (const char *)s2;

    /*  Overlapping like this:
        <src----->
            <dest--->
        requires a backwards copy, or else we'll overwrite data before we copy it.
        Non overlapping can use just a memcpy, and overlapping like this NEEDS to use it:
        <dest--->
           <src---->
        so we just call memcpy for both. */
    if (Source >= Destination || Source + n <= Destination) {
        return memcpy(s1, s2, n);
    }

    Destination += n;
    Source += n;

    while (n--) {
        *(--Destination) = *(--Source);
    }

    return s1;
}
