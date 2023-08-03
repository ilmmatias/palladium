/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a standard complying version of `formatting into a string`.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int sprintf(char *buffer, const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vsprintf(buffer, format, vlist);
    va_end(vlist);
    return size;
}
