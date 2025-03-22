/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to concatenate a C string at the end of another.
 *
 * PARAMETERS:
 *     s1 - Destination buffer.
 *     s2 - Source buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strcat(char *restrict s1, const char *restrict s2) {
    char *Destination = s1;
    const char *Source = s2;

    while (*Destination) {
        Destination++;
    }

    while (*Source) {
        *(Destination++) = *(Source++);
    }

    *Destination = 0;

    return s1;
}
