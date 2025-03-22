/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the first occourence of a byte.
 *
 * PARAMETERS:
 *     s - Buffer to search the byte in.
 *     c - Which byte to search for.
 *     n - Size of the buffer.
 *
 * RETURN VALUE:
 *     Pointer to where the byte was found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
void *memchr(const void *s, int c, size_t n) {
    const char *Buffer = s;

    while (*Buffer != c && n) {
        Buffer++;
        n--;
    }

    return n ? (void *)Buffer : NULL;
}
