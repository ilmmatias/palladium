/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fputc(stdout).
 *
 * PARAMETERS:
 *     ch - Which character to write.
 *
 * RETURN VALUE:
 *     Whatever stored in `ch` on success, EOF otherwise.
 *-----------------------------------------------------------------------------------------------*/
int putchar(int ch) {
    return fputc(ch, stdout);
}
