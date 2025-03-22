/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the first occourence of a C string inside
 *     another.
 *
 * PARAMETERS:
 *     s1 - Which string to analyze.
 *     s2 - String to search for.
 *
 * RETURN VALUE:
 *     Position where the string was found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
char *strstr(const char *s1, const char *s2) {
    if (!*s2) {
        return (char *)s1;
    }

    while (*s1) {
        const char *Search = s2;
        const char *Start = s1;

        while (*s1 && *Search && *(s1++) == *Search) {
            Search++;
        }

        if (!*Search) {
            return (char *)Start;
        }
    }

    return NULL;
}
