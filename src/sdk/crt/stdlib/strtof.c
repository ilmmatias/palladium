/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a float from a string.
 *
 * PARAMETERS:
 *     nptr - String to be parsed.
 *     endptr - Output; pointer to after the last valid character.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid float, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
float strtof(const char *restrict nptr, char **restrict endptr) {
    return (float)strtod(nptr, endptr);
}
