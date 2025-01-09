/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to compare two C strings, assuming that the string
 *     with the smallest buffer has size `count`.
 *
 * PARAMETERS:
 *     lhs - Left-hand side of the expression.
 *     rhs - Right-hand side of the expression.
 *     count - Size of the smallest string.
 *
 * RETURN VALUE:
 *     0 for equality, *lhs - *rhs if an inequality is found (where `lhs` and `rhs` would be
 *     pointers to where the inequality happened).
 *-----------------------------------------------------------------------------------------------*/
int strncmp(const char *lhs, const char *rhs, size_t count) {
    while (count--) {
        if (!*lhs || !*rhs || *lhs != *rhs) {
            return *lhs - *rhs;
        }

        lhs++;
        rhs++;
    }

    return 0;
}
