/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the first occourence of a character.
 *
 * PARAMETERS:
 *     str - Where to search for the character.
 *     ch - Which character to search for.
 *
 * RETURN VALUE:
 *     Pointer to where the character was found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
char *strchr(const char *str, int ch) {
    while (*str && *str != ch) {
        str++;
    }

    return *str ? (char *)str : NULL;
}
