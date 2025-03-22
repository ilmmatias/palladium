/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to fill a region of memory with a specific byte.
 *
 * PARAMETERS:
 *     s - Destination buffer.
 *     c - Byte to be used as pattern.
 *     n - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memset(void *s, int c, size_t n) {
    char *Destination = s;

    while (n--) {
        *(Destination++) = c;
    }

    return s;
}
