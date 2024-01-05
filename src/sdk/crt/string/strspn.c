/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to find how many characters from the `src` set does
 *     the `dest` string start with.
 *
 * PARAMETERS:
 *     dest - Which string to analyze.
 *     src - Search set.
 *
 * RETURN VALUE:
 *     How many characters from the source set the string starts with.
 *-----------------------------------------------------------------------------------------------*/
size_t strspn(const char *dest, const char *src) {
    size_t Count = 0;

    while (*dest && strchr(src, *(dest++))) {
        Count++;
    }

    return Count;
}
