/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores the file position back to 0; Effectively equal to
 *     `fseek_unlocked(stream, 0, SEEK_SET)`.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void rewind_unlocked(FILE *stream) {
    fseek_unlocked(stream, 0, SEEK_SET);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores the file position back to 0; Effectively equal to
 *     `fseek(stream, 0, SEEK_SET)`.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void rewind(FILE *stream) {
    fseek(stream, 0, SEEK_SET);
}
