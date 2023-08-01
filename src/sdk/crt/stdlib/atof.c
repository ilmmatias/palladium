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
 *     Result of the parsing if the string was a valid double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
double atof_hex(const char *str, double sign) {
    double value = 0.0;
    int dot = 0;
    int p = 0;

    /* We combine handling the digits before and after the dot. */
    while (isxdigit(*str) || (!dot && *str == '.')) {
        int digit = *(str++);

        if (digit == '.') {
            dot = 1;
            continue;
        } else if (dot) {
            /* Each digit is a nibble (4-bits). */
            p -= 4;
        }

        value *= 16.;

        /* First two cases are aA-fF, offset by +0x0A. */
        if (digit > 0x60) {
            value += digit - 0x57;
        } else if (digit > 0x40) {
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
        value *= 16.;
        p--;
    }

    while (p++ < 0) {
        value *= 0.5;
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
 *     Result of the parsing if the string was a valid double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
double atof_dec(const char *str, double sign) {
    double value = 0.0;
    int dot = 0;
    int e = 0;

    /* We combine handling the digits before and after the dot. */
    while (isdigit(*str) || (!dot && *str == '.')) {
        int digit = *(str++);

        if (digit == '.') {
            dot = 1;
            continue;
        } else if (dot) {
            e -= 1;
        }

        value = value * 10. + digit - 0x30;
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
        value *= 10.;
        e--;
    }

    while (e++ < 0) {
        value *= 0.1;
    }

    return sign * value;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a double (floating-point value) from a string.
 *
 * PARAMETERS:
 *     str - String to be parsed.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid double, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
double atof(const char *str) {
    double sign = *str == '-' ? -1. : 1.;

    if (sign < 0 || *str == '+') {
        str++;
    }

    if (*str == '0' && (*(str + 1) == 'x' || *(str + 1) == 'X')) {
        return atof_hex(str + 2, sign);
    }

    if (isdigit(*str)) {
        return atof_dec(str, sign);
    }

    if ((*str == 'i' || *str == 'I') && (*(str + 1) == 'n' || *(str + 1) == 'N') &&
        (*(str + 2) == 'f' || *(str + 2) == 'F')) {
        return sign < 0 ? -INFINITY : INFINITY;
    }

    /* TODO: We should also support quiet NaNs in the future. */
    if ((*str == 'n' || *str == 'N') && (*(str + 1) == 'a' || *(str + 1) == 'A') &&
        (*(str + 2) == 'n' || *(str + 2) == 'N')) {
        return sign < 0 ? -NAN : NAN;
    }

    return 0.0;
}
