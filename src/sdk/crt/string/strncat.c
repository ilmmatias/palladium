/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to concatenate a C string at the end of another,
 *     while assuming that the destination buffer can store at most an extra `n` bytes.
 *
 * PARAMETERS:
 *     s1 - Destination buffer.
 *     s2 - Source buffer.
 *     n - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strncat(char *CRT_RESTRICT s1, const char *CRT_RESTRICT s2, size_t n) {
    char *Destination = s1;
    const char *Source = s2;

    while (*Destination) {
        Destination++;
    }

    while (*Source && n) {
        *(Destination++) = *(Source++);
        n--;
    }

    if (n) {
        *Destination = 0;
    }

    return s1;
}
