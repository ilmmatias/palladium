/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to compare the data in two buffers.
 *
 * PARAMETERS:
 *     s1 - Left-hand side of the expression.
 *     s2 - Right-hand side of the expression.
 *     n - Size of the smallest buffer.
 *
 * RETURN VALUE:
 *     0 for equality, *s1 - *s2 if an inequality is found (where `s1` and `s1` are both
 *     reinterpreted as char*, and are pointing to where the inequality happened).
 *-----------------------------------------------------------------------------------------------*/
int memcmp(const void *s1, const void *s2, size_t n) {
    const char *Left = s1;
    const char *Right = s2;

    while (n--) {
        if (*Left != *Right) {
            return *Left - *Right;
        }

        Left++;
        Right++;
    }

    return 0;
}
