/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to determine the size of a C string.
 *
 * PARAMETERS:
 *     str - Source string.
 *
 * RETURN VALUE:
 *     Size in bytes of the given string (excluding the \0 at the end).
 *-----------------------------------------------------------------------------------------------*/
size_t strlen(const char *str) {
    size_t Size = 0;

    while (*(str++)) {
        Size++;
    }

    return Size;
}
