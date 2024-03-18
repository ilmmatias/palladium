/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fgetc(stdin).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Data read from stdin, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int getchar(void) {
    return fgetc(stdin);
}
