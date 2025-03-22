/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy a C string to another buffer, while assuming
 *     that the destination buffer can store at most `n` bytes.
 *
 * PARAMETERS:
 *     s1 - Destination buffer.
 *     s2 - Source buffer.
 *     n - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strncpy(char *restrict s1, const char *restrict s2, size_t n) {
    char *Destination = s1;
    const char *Source = s2;

    while (*Source && n) {
        *(Destination++) = *(Source++);
        n--;
    }

    if (n) {
        *Destination = 0;
    }

    return s1;
}
