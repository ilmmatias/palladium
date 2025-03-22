/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the position of the first character from the
 *     `s2` in the `s1` string.
 *
 * PARAMETERS:
 *     s1 - Which string to analyze.
 *     s2 - Search set.
 *
 * RETURN VALUE:
 *     Position of the found character, or NULL if none was found.
 *-----------------------------------------------------------------------------------------------*/
char *strpbrk(const char *s1, const char *s2) {
    while (*s1 && !strchr(s2, *s1)) {
        s1++;
    }

    return *s1 ? (char *)s1 : NULL;
}
