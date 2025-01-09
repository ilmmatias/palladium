/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ctype.h>
#include <limits.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses an integer (more specifically, an `unsigned long`) from a string.
 *
 * PARAMETERS:
 *     str - String to be parsed.
 *     str_end - Output; pointer to after the last valid character.
 *     base - Which base the number is in, or 0 for auto detection.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid integer, or 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
unsigned long strtoul(const char *str, char **str_end, int base) {
    const char *start = str;
    while (isspace(*str)) {
        str++;
    }

    int sign = *str == '-';
    if (sign || *str == '+') {
        str++;
    }

    /* Don't skip the prefix on auto detection, we'll do that below, as it is also allowed
       without auto detection. */
    if (base == 0) {
        if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) {
            base = 16;
        } else if (*str == '0') {
            base = 8;
        } else {
            base = 10;
        }
    }

    if (base == 16 && *str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) {
        str += 2;
    }

    const char *after_prefixes = str;
    unsigned long value = 0;
    int error = 0;

    while (1) {
        unsigned long last = value;
        int digit = *str;

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
                value = ULONG_MAX;
                error = 1;
            }
        }

        str++;
    }

    if (after_prefixes == str) {
        error = 1;
        value = 0;
        str = start;
    }

    if (str_end) {
        *str_end = (char *)str;
    }

    return sign && !error ? -value : value;
}
