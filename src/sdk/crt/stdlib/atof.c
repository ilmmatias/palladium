/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/fmt.h>
#include <ctype.h>
#include <math.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a double (floating-point value) from a string.
 *
 * PARAMETERS:
 *     nptr - String to be parsed.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
double atof(const char *nptr) {
    double sign = *nptr == '-' ? -1. : 1.;

    if (sign < 0 || *nptr == '+') {
        nptr++;
    }

    if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
        return __strtod_hex(nptr + 2, sign);
    }

    if (isdigit(*nptr)) {
        return __strtod_dec(nptr, sign);
    }

    if ((*nptr == 'i' || *nptr == 'I') && (*(nptr + 1) == 'n' || *(nptr + 1) == 'N') &&
        (*(nptr + 2) == 'f' || *(nptr + 2) == 'F')) {
        return sign < 0 ? -INFINITY : INFINITY;
    }

    /* TODO: We should also support quiet NaNs in the future. */
    if ((*nptr == 'n' || *nptr == 'N') && (*(nptr + 1) == 'a' || *(nptr + 1) == 'A') &&
        (*(nptr + 2) == 'n' || *(nptr + 2) == 'N')) {
        return sign < 0 ? -NAN : NAN;
    }

    return 0.;
}
