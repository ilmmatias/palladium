/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy a C string to another buffer, while assuming
 *     that the destination buffer can store at most `count` bytes.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     count - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strncpy(char *dest, const char *src, size_t count) {
    char *Destination = dest;
    const char *Source = src;

    while (*Source && count) {
        *(Destination++) = *(Source++);
        count--;
    }

    if (count) {
        *Destination = 0;
    }

    return dest;
}
