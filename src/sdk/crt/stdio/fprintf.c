/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into a FILE. Unlike the normal variant, we should only
 *     be called after acquiring the file lock.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int fprintf_unlocked(FILE *restrict stream, const char *restrict format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vfprintf_unlocked(stream, format, vlist);
    va_end(vlist);
    return size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into a FILE.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int fprintf(FILE *restrict stream, const char *restrict format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vfprintf(stream, format, vlist);
    va_end(vlist);
    return size;
}
