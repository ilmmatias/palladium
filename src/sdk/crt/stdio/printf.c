/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into the standard output. Unlike the normal variant, we
 *     should only be called after acquiring the file lock.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int printf_unlocked(const char *CRT_RESTRICT format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vprintf_unlocked(format, vlist);
    va_end(vlist);
    return size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into the standard output.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int printf(const char *CRT_RESTRICT format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vprintf(format, vlist);
    va_end(vlist);
    return size;
}
