/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a string followed by a new-line character to the screen. Unlike the
 *     normal variant, we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     str - The string to be written.
 *
 * RETURN VALUE:
 *     How many characters we wrote on success, EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int puts_unlocked(const char *str) {
    int accum = fputs_unlocked(str, stdout);

    if (accum == EOF || fputc_unlocked('\n', stdout) == EOF) {
        return EOF;
    }

    return accum + 1;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a string followed by a new-line character to the screen.
 *
 * PARAMETERS:
 *     str - The string to be written.
 *
 * RETURN VALUE:
 *     How many characters we wrote on success, EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int puts(const char *str) {
    flockfile(stdout);
    int res = puts_unlocked(str);
    funlockfile(stdout);
    return res;
}
