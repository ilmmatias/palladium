/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <ctype.h>
#include <math.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is called when a hexadecimal float has been found and needs to be parsed.
 *     The input needs to have already skipped the `0x` prefix.
 *
 * PARAMETERS:
 *     str - String to be parsed.
 *     sign - Sign of the expression.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid long double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static long double parse_hex(const char *str, long double sign) {
    long double value = 0.L;
    bool dot = false;
    int p = 0;

    /* We combine handling the digits before and after the dot. */
    while (isxdigit(*str) || (!dot && *str == '.')) {
        int digit = *(str++);

        if (digit == '.') {
            dot = true;
            continue;
        } else if (dot) {
            /* Each digit is a nibble (4-bits). */
            p -= 4;
        }

        value *= 16.L;

        /* First two cases are aA-fF, offset by +0x0A. */
        if (islower(digit)) {
            value += digit - 0x57;
        } else if (isupper(digit)) {
            value += digit - 0x37;
        } else {
            value += digit - 0x30;
        }
    }

    if (*str == 'p' || *str == 'P') {
        int p_sign = *(++str) == '-' ? -1 : 1;
        int p_val = 0;

        if (p_sign < 0 || *str == '+') {
            str++;
        }

        while (isdigit(*str)) {
            p_val = p_val * 10 + *(str++) - 0x30;
        }

        p += p_sign * p_val;
    }

    while (p > 0) {
        value *= 16.L;
        p--;
    }

    while (p++ < 0) {
        value *= .5L;
    }

    return sign * value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is called when a decimal float has been found and needs to be parsed.
 *
 * PARAMETERS:
 *     str - String to be parsed.
 *     sign - Sign of the expression.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid long double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
static long double parse_dec(const char *str, long double sign) {
    long double value = 0.L;
    bool dot = false;
    int e = 0;

    /* We combine handling the digits before and after the dot. */
    while (isdigit(*str) || (!dot && *str == '.')) {
        int digit = *(str++);

        if (digit == '.') {
            dot = true;
            continue;
        } else if (dot) {
            e -= 1;
        }

        value = value * 10.L + digit - 0x30;
    }

    if (*str == 'e' || *str == 'E') {
        int e_sign = *(++str) == '-' ? -1 : 1;
        int e_val = 0;

        if (e_sign < 0 || *str == '+') {
            str++;
        }

        while (isdigit(*str)) {
            e_val = e_val * 10 + *(str++) - 0x30;
        }

        e += e_sign * e_val;
    }

    while (e > 0) {
        value *= 10.L;
        e--;
    }

    while (e++ < 0) {
        value *= .1L;
    }

    return sign * value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a long double (floating-point value) from a string.
 *     Differently from atof, this handles whitespaces in the start.
 *
 * PARAMETERS:
 *     nptr - String to be parsed.
 *     endptr - Output; pointer to after the last valid character.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid long double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
long double strtold(const char *CRT_RESTRICT nptr, char **CRT_RESTRICT endptr) {
    const char *start = nptr;
    while (isspace(*nptr)) {
        nptr++;
    }

    long double sign = *nptr == '-' ? -1.L : 1.L;
    if (sign < 0 || *nptr == '+') {
        nptr++;
    }

    long double value = 0.L;

    if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
        if (!isxdigit(*(nptr + 2))) {
            nptr = start;
        } else {
            value = parse_hex(nptr + 2, sign);
        }
    } else if (isdigit(*nptr)) {
        value = parse_dec(nptr, sign);
    } else if (
        (*nptr == 'i' || *nptr == 'I') && (*(nptr + 1) == 'n' || *(nptr + 1) == 'N') &&
        (*(nptr + 2) == 'f' || *(nptr + 2) == 'F')) {
        value = sign < 0 ? -INFINITY : INFINITY;
    } else if (
        (*nptr == 'n' || *nptr == 'N') && (*(nptr + 1) == 'a' || *(nptr + 1) == 'A') &&
        (*(nptr + 2) == 'n' || *(nptr + 2) == 'N')) {
        /* TODO: We should also support quiet NaNs in the future. */
        value = sign < 0 ? -NAN : NAN;
    } else {
        nptr = start;
    }

    if (endptr) {
        *endptr = (char *)nptr;
    }

    return value;
}
