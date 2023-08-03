/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the first occourence of a byte.
 *
 * PARAMETERS:
 *     ptr - Buffer to search the byte in.
 *     ch - Which byte to search for.
 *     count - Size of the buffer.
 *
 * RETURN VALUE:
 *     Pointer to where the byte was found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
void *memchr(const void *ptr, int ch, size_t count) {
    const char *Buffer = ptr;

    while (*Buffer != ch && count) {
        Buffer++;
        count--;
    }

    return count ? (void *)Buffer : NULL;
}
