/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fgetc_unlocked(stdin).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Data read from stdin, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int getchar_unlocked(void) {
    return fgetc_unlocked(stdin);
}

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
