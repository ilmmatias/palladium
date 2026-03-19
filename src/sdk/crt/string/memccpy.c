/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy bytes from one region of memory to another,
 *     stopping after copying the specified amount or after copying the specified byte.
 *
 * PARAMETERS:
 *     s1 - Destination buffer.
 *     s2 - Source buffer.
 *     c - Marker.
 *     n - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Pointer to the byte after the marker, or NULL if the marker wasn't found.
 *-----------------------------------------------------------------------------------------------*/
void *memccpy(void *CRT_RESTRICT s1, const void *CRT_RESTRICT s2, int c, size_t n) {
    unsigned char *Destination = s1;
    const unsigned char *Source = s2;
    unsigned char Marker = c;

    while (n) {
        unsigned char Data = *(Source++);
        *(Destination++) = Data;

        if (Data == Marker) {
            break;
        }

        n--;
    }

    return n ? Destination : NULL;
}
