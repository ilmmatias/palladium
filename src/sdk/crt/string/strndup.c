/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to create a copy of a C string, limiting the newly
 *     allocated string to at most `n` bytes + 1 (space for the null terminator).
 *
 * PARAMETERS:
 *     s - The string to be copied.
 *     n - How many bytes are we allowed to copy at most.
 *
 * RETURN VALUE:
 *     Copy of the source string, or NULL if we couldn't allocate the memory.
 *-----------------------------------------------------------------------------------------------*/
char *strndup(const char *s, size_t n) {
    /* The source string is allowed to not end with a null terminator, so we can't use
       strlen(). */

    const char *Source = s;
    size_t CopySize = 0;

    while (*(Source++) && n--) {
        CopySize++;
    }

    char *Copy = malloc(CopySize + 1);
    if (Copy) {
        memcpy(Copy, s, CopySize);
        Copy[CopySize] = 0;
    }

    return Copy;
}
