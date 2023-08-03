/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ctype.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses an integer (more specifically, a `long long`) from a string.
 *
 * PARAMETERS:
 *     str - String to be parsed.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid integer, or 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
long long atoll(const char *str) {
    long long sign = *str == '-' ? -1 : 1;
    long long value = 0;

    if (sign < 0 || *str == '+') {
        str++;
    }

    while (isdigit(*str)) {
        value = value * 10 + *(str++) - 0x30;
    }

    return sign * value;
}
