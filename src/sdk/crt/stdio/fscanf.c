/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from a FILE based on a format string. Unlike the normal
 *     variant, we should only be called after acquiring the file lock.
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
int fscanf_unlocked(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vfscanf_unlocked(stream, format, vlist);
    va_end(vlist);
    return size;
}

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
int fscanf(FILE *CRT_RESTRICT stream, const char *CRT_RESTRICT format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vfscanf(stream, format, vlist);
    va_end(vlist);
    return size;
}
