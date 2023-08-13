/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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
