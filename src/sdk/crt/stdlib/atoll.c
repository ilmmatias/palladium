/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses an integer (more specifically, a `long long`) from a string.
 *
 * PARAMETERS:
 *     nptr - String to be parsed.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid integer, or 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
long long atoll(const char *nptr) {
    long long sign = *nptr == '-' ? -1 : 1;
    long long value = 0;

    if (sign < 0 || *nptr == '+') {
        nptr++;
    }

    while (isdigit(*nptr)) {
        value = value * 10 + *(nptr++) - 0x30;
    }

    return sign * value;
}
