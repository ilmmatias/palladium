/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/fmt.h>
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
    return fgetc_unlocked((FILE *)context);
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
static void unread_ch(void *context, int ch) {
    ungetc_unlocked(ch, (FILE *)context);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from a FILE based on a format string.
 *     While fscanf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     Unlike the normal variant, we should only be called after acquiring the file lock.
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
int vfscanf_unlocked(FILE *restrict stream, const char *restrict format, va_list arg) {
    return __vscanf(format, arg, stream, read_ch, unread_ch);
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
int vfscanf(FILE *restrict stream, const char *restrict format, va_list arg) {
    flockfile(stream);
    int res = vfscanf_unlocked(stream, format, arg);
    funlockfile(stream);
    return res;
}
