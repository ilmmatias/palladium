/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from the standard input based on a format string.
 *     While scanf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     Unlike the normal variant, we should only be called after acquiring the file lock.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     arg - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int vscanf_unlocked(const char *CRT_RESTRICT format, va_list arg) {
    return vfscanf_unlocked(stdin, format, arg);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from the standard input based on a format string.
 *     While scanf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     arg - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int vscanf(const char *CRT_RESTRICT format, va_list arg) {
    return vfscanf(stdin, format, arg);
}
