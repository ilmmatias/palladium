/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from a string based on another format string.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     s - Output buffer.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int sscanf(const char *restrict s, const char *restrict format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vsscanf(s, format, vlist);
    va_end(vlist);
    return size;
}
