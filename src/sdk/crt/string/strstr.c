/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the first occourence of a C string inside
 *     another.
 *
 * PARAMETERS:
 *     str - Which string to analyze.
 *     substr - String to search for.
 *
 * RETURN VALUE:
 *     Position where the string was found, or NULL if it wasn't found.
 *-----------------------------------------------------------------------------------------------*/
char *strstr(const char *str, const char *substr) {
    if (!*substr) {
        return (char *)str;
    }

    while (*str) {
        const char *Search = substr;
        const char *Start = str;

        while (*str && *Search && *(str++) == *Search) {
            Search++;
        }

        if (!*Search) {
            return (char *)Start;
        }
    }

    return NULL;
}
