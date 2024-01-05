/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the last occourence of a character.
 *
 * PARAMETERS:
 *     str - Where to search for the character.
 *     ch - Which character to search for.
 *
 * RETURN VALUE:
 *     Pointer to where the character was last found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
char *strrchr(const char *str, int ch) {
    const char *Last = NULL;

    while (*str) {
        if (*str == ch) {
            Last = str;
        }

        str++;
    }

    return (char *)Last;
}
