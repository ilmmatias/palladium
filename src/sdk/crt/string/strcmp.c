/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to compare two C strings.
 *
 * PARAMETERS:
 *     s1 - Left-hand side of the expression.
 *     s2 - Right-hand side of the expression.
 *
 * RETURN VALUE:
 *     0 for equality, *s1 - *s2 if an inequality is found (where `s1` and `s2` would be pointers
 *     to where the inequality happened).
 *-----------------------------------------------------------------------------------------------*/
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }

    return *s1 - *s2;
}
