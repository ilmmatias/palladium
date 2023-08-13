/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from the standard input based on a format string.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int scanf(const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vscanf(format, vlist);
    va_end(vlist);
    return size;
}
