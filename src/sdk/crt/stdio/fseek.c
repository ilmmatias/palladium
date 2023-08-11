/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function updates the current FILE position based on the specified `offset` from the
 *     `origin` point within the stream.
 *
 * PARAMETERS:
 *     stream - Input file stream.
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

    stream->flags &= ~__STDIO_FLAGS_EOF;
    stream->unget_size = 0;
    stream->file_pos = pos;
    stream->buffer_file_pos = pos;
    stream->buffer_pos = 0;

    return 0;
}
