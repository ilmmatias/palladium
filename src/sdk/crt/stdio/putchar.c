/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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
