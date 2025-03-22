/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find how many characters not in the `s2` set
 *     does the `s1` string start with.
 *
 * PARAMETERS:
 *     s1 - Which string to analyze.
 *     s2 - Search set.
 *
 * RETURN VALUE:
 *     How many characters not in the source set the string starts with.
 *-----------------------------------------------------------------------------------------------*/
size_t strcspn(const char *s1, const char *s2) {
    size_t Count = 0;

    while (*s1 && !strchr(s2, *(s1++))) {
        Count++;
    }

    return Count;
}
