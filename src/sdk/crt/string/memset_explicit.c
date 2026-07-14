/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements code to fill a region of memory with a specific byte, in a way
 *     that can be used to clear sensitive regions of memory.
 *
 * PARAMETERS:
 *     s - Destination buffer.
 *     c - Byte to be used as pattern.
 *     n - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memset_explicit(void *s, int c, size_t n) {
    /* I think doing a compiler barrier like this should safe (as the actual asm string is
     * empty)? */
    memset(s, c, n);
    __asm__ volatile("" : : "g"(&s) : "memory");
    return s;
}
