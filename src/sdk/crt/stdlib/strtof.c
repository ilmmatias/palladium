/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses a float from a string.
 *
 * PARAMETERS:
 *     str - String to be parsed.
 *     str_end - Output; pointer to after the last valid character.
 *
 * RETURN VALUE:
 *     Result of the parsing if the string was a valid float, 0.0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
float strtof(const char *str, char **str_end) {
    return (float)strtod(str, str_end);
}
