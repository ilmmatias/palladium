/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function updates the current FILE position based on the specified `offset` from the
 *     `origin` point within the stream.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     offset - How far away from the `origin` we want to go.
 *     origin - Which point of the file we want to use as base (SET/start, CUR/current, END/end).
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fseek(struct FILE *stream, long offset, int origin) {
    size_t pos;

    if (!stream) {
        return 1;
    }

    /* Assume unknown values are SEEK_SET, instead of rejecting them. */
    switch (origin) {
        case SEEK_CUR:
            pos = stream->file_pos + offset;
            break;
        case SEEK_END:
            pos = stream->file_size + offset;
            break;
        default:
            pos = offset;
            break;
    }

    fflush(stream);
    stream->flags &= ~__STDIO_FLAGS_EOF;
    stream->file_pos = pos;
    stream->buffer_file_pos = pos;

    return 0;
}
