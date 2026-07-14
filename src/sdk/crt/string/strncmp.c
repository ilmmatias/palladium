/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to compare two C strings, assuming that the string
 *     with the smallest buffer has size `n`.
 *
 * PARAMETERS:
 *     s1 - Left-hand side of the expression.
 *     s2 - Right-hand side of the expression.
 *     n - Size of the smallest string.
 *
 * RETURN VALUE:
 *     0 for equality, *s1 - *s2 if an inequality is found (where `s1` and `s2` would be pointers
 *     to where the inequality happened).
 *-----------------------------------------------------------------------------------------------*/
__attribute__((no_builtin)) int strncmp(const char *s1, const char *s2, size_t n) {
    const unsigned char *Left = (const unsigned char *)s1;
    const unsigned char *Right = (const unsigned char *)s2;

    while (n) {
        if (!*Left || !*Right || *Left != *Right) {
            return *Left - *Right;
        }

        Left++;
        Right++;
        n--;
    }

    return 0;
}
