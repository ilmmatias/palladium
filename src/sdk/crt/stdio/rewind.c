/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores the file position back to 0; Effectively equal to
 *     `fseek(stream, 0, SEEK_SET)`.
 *
 * PARAMETERS:
 *     stream - Input file stream.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int rewind(struct FILE *stream) {
    if (!stream) {
        return 1;
    }

    stream->flags &= ~__STDIO_FLAGS_EOF;
    stream->unget_size = 0;
    stream->file_pos = 0;
    stream->buffer_file_pos = 0;
    stream->buffer_pos = 0;

    return 0;
}
