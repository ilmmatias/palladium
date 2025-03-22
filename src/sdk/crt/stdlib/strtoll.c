/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <limits.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses an integer (more specifically, a `long long`) from a string.
 *     Differently from atol, this supports identifying (or manually detecting) the base, and
 *     it also skips whitespaces from the start.
 *
 * PARAMETERS:
 *     nptr - String to be parsed.
 *     endptr - Output; pointer to after the last valid character.
 *     base - Which base the number is in, or 0 for auto detection.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid integer, or 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
long long strtoll(const char *restrict nptr, char **restrict endptr, int base) {
    const char *start = nptr;
    while (isspace(*nptr)) {
        nptr++;
    }

    long long sign = *nptr == '-' ? -1 : 1;
    if (sign < 0 || *nptr == '+') {
        nptr++;
    }

    /* Don't skip the prefix on auto detection, we'll do that below, as it is also allowed
       without auto detection. */
    if (base == 0) {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            base = 16;
        } else if (*nptr == '0') {
            base = 8;
        } else {
            base = 10;
        }
    }

    if (base == 16 && *nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
        nptr += 2;
    }

    const char *after_prefixes = nptr;
    long long value = 0;
    bool error = false;

    while (true) {
        long long last = value;
        int digit = *nptr;

        /* First two cases are aA-fF, offset by +0x0A. */
        if (islower(digit)) {
            digit -= 0x57;
        } else if (isupper(digit)) {
            digit -= 0x37;
        } else if (isdigit(digit)) {
            digit -= 0x30;
        } else {
            break;
        }

        if (digit >= base) {
            break;
        }

        if (!error) {
            value = (value * base) + digit;
            if (value < last) {
                value = LLONG_MAX;
                error = true;
            }
        }

        nptr++;
    }

    if (after_prefixes == nptr) {
        error = true;
        value = 0;
        nptr = start;
    }

    if (endptr) {
        *endptr = (char *)nptr;
    }

    return error ? value : sign * value;
}
