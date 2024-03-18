/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles writting the buffer into the given FILE.
 *
 * PARAMETERS:
 *     buffer - Input (character) buffer.
 *     size - Input buffer size.
 *     context - Expected to be the FILE pointer.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static void put_buf(const void *buffer, int size, void *context) {
    fwrite(buffer, size, 1, (FILE *)context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into a FILE.
 *     While fprintf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     format - Base format string.
 *     vlist - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int vfprintf(FILE *stream, const char *format, va_list vlist) {
    return __vprintf(format, vlist, stream, put_buf);
}
