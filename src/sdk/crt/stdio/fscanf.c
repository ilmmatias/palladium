/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from a FILE based on a format string.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int fscanf(FILE *stream, const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vfscanf(stream, format, vlist);
    va_end(vlist);
    return size;
}
