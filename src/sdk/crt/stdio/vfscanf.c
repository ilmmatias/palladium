/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles reading the next character from the file (wrapper around fgetc).
 *
 * PARAMETERS:
 *     context - Expected to be a FILE pointer.
 *
 * RETURN VALUE:
 *     Same result value as fgetc.
 *-----------------------------------------------------------------------------------------------*/
static int read_ch(void *context) {
    return fgetc((FILE *)context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles rewinding the buffer (wrapper around ungetc).
 *
 * PARAMETERS:
 *     context - Expected to be a FILE pointer.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void unread_ch(void *context, int ch) {
    ungetc(ch, (FILE *)context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from a FILE based on a format string.
 *     While fscanf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     format - Base format string.
 *     vlist - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int vfscanf(FILE *stream, const char *format, va_list vlist) {
    return __vscanf(format, vlist, stream, read_ch, unread_ch);
}
