/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the first occourence of a character.
 *
 * PARAMETERS:
 *     s - Where to search for the character.
 *     c - Which character to search for.
 *
 * RETURN VALUE:
 *     Pointer to where the character was found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
__attribute__((no_builtin)) char *strchr(const char *s, int c) {
    unsigned char Data = c;

    while ((unsigned char)*s != Data) {
        if (!*s) {
            return NULL;
        }

        s++;
    }

    return (char *)s;
}
