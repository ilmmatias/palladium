/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a standard complying version of `formatting into a string`.
 *     Differntly from sprintf(), this allows setting the max amount of data to output, and allows
 *     the buffer to be NULL as long as the max size is set to 0.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     bufsz - Size of the output buffer.
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int snprintf(char *buffer, size_t bufsz, const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vsnprintf(buffer, bufsz, format, vlist);
    va_end(vlist);
    return size;
}
