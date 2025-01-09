/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores the file position back to 0; Effectively equal to
 *     `fseek(stream, 0, SEEK_SET)`.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int rewind(struct FILE *stream) {
    if (!stream) {
        return 1;
    }

    fflush(stream);
    stream->flags &= ~__STDIO_FLAGS_EOF;
    stream->file_pos = 0;

    return 0;
}
