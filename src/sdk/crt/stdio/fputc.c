/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fwrite_unlocked(&ch, 1, 1, stream), returning either `ch`, or EOF on failure.
 *
 * PARAMETERS:
 *     ch - What to write into the file.
 *     stream - FILE stream; Needs to be open with the write flag.
 *
 * RETURN VALUE:
 *     Data written onto the file, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fputc_unlocked(int ch, FILE *stream) {
    return fwrite_unlocked(&ch, 1, 1, stream) ? ch : EOF;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes a character into the file stream.
 *
 * PARAMETERS:
 *     ch - What to write into the file.
 *     stream - FILE stream; Needs to be open with the write flag.
 *
 * RETURN VALUE:
 *     Data written onto the file, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fputc(int ch, FILE *stream) {
    flockfile(stream);
    int res = fputc_unlocked(ch, stream);
    funlockfile(stream);
    return res;
}
