/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the last occourence of a character.
 *
 * PARAMETERS:
 *     s - Where to search for the character.
 *     ch - Which character to search for.
 *
 * RETURN VALUE:
 *     Pointer to where the character was last found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
char *strrchr(const char *s, int c) {
    const char *Last = NULL;
    unsigned char Data = c;

    do {
        if ((unsigned char)*s == Data) {
            Last = s;
        }

        s++;
    } while (*(s - 1));

    return (char *)Last;
}
