
/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into a string.
 *     Differently from sprintf(), this allows setting the max amount of data to output, and allows
 *     the buffer to be NULL as long as the max size is set to 0.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     s - Output buffer.
 *     bufsz - Size of the output buffer.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int snprintf(char *CRT_RESTRICT s, size_t bufsz, const char *CRT_RESTRICT format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vsnprintf(s, bufsz, format, vlist);
    va_end(vlist);
    return size;
}
