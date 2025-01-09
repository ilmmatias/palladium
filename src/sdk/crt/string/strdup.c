/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to create a copy of a C string.
 *
 * PARAMETERS:
 *     src - The string to be copied.
 *
 * RETURN VALUE:
 *     Copy of the source string, or NULL if we couldn't allocate the memory.
 *-----------------------------------------------------------------------------------------------*/
char *strdup(const char *src) {
    if (!src) {
        return NULL;
    }

    size_t Size = strlen(src) + 1;
    char *Copy = malloc(Size);

    if (Copy) {
        memcpy(Copy, src, Size);
    }

    return Copy;
}
