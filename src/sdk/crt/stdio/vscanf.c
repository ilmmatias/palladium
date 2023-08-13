/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from the standard input based on a format string.
 *     While scanf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     vlist - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int vscanf(const char *format, va_list vlist) {
    return vfscanf(stdin, format, vlist);
}
