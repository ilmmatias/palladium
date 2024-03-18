/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find the position of the first character from the
 *     `breakset` in the `dest` string.
 *
 * PARAMETERS:
 *     dest - Which string to analyze.
 *     breakset - Search set.
 *
 * RETURN VALUE:
 *     Position of the found character, or NULL if none was found.
 *-----------------------------------------------------------------------------------------------*/
char *strpbrk(const char *dest, const char *breakset) {
    while (*dest && !strchr(breakset, *dest)) {
        dest++;
    }

    return *dest ? (char *)dest : NULL;
}
