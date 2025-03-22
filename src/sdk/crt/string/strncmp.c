/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

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
int strncmp(const char *s1, const char *s2, size_t n) {
    while (n--) {
        if (!*s1 || !*s2 || *s1 != *s2) {
            return *s1 - *s2;
        }

        s1++;
        s2++;
    }

    return 0;
}
