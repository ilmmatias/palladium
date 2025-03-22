/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into the standard output.
 *     While printf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     Unlike the normal variant, we should only be called after acquiring the file lock.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     format - Base format string.
 *     arg - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int vprintf_unlocked(const char *restrict format, va_list arg) {
    return vfprintf_unlocked(stdout, format, arg);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into the standard output.
 *     While printf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     format - Base format string.
 *     arg - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int vprintf(const char *restrict format, va_list arg) {
    return vfprintf(stdout, format, arg);
}
