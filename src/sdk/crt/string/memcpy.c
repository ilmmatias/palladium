/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy bytes from one region of memory to another.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     count - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
    char *restrict Destination = dest;
    const char *restrict Source = src;

    while (count--) {
        *(Destination++) = *(Source++);
    }

    return dest;
}
