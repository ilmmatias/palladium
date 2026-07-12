/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to determine the size of a C string.
 *
 * PARAMETERS:
 *     s - Source string.
 *
 * RETURN VALUE:
 *     Size in bytes of the given string (excluding the \0 at the end).
 *-----------------------------------------------------------------------------------------------*/
__attribute__((no_builtin)) size_t strlen(const char *s) {
    size_t Size = 0;

    while (*(s++)) {
        Size++;
    }

    return Size;
}
