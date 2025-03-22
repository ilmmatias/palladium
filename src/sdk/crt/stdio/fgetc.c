/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fread_unlocked(&ch, 1, 1, stream), returning either `ch`, or EOF on failure.
 *
 * PARAMETERS:
 *     stream - FILE stream; Needs to be open with the read flag.
 *
 * RETURN VALUE:
 *     Data stored in the fread buffer, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fgetc_unlocked(FILE *stream) {
    int ch = 0;
    return fread_unlocked(&ch, 1, 1, stream) ? ch : EOF;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads the next character in the file stream.
 *
 * PARAMETERS:
 *     stream - FILE stream; Needs to be open with the read flag.
 *
 * RETURN VALUE:
 *     Either the data read from the stream, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fgetc(FILE *stream) {
    flockfile(stream);
    int res = fgetc_unlocked(stream);
    funlockfile(stream);
    return res;
}
