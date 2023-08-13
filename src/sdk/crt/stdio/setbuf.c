/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function overwrites the current file buffer. It is assumed that the buffer is either
 *     NULL (disable buffering), or at least BUFSIZ bytes longer; If that is not the case, use
 *     setvbuf() instead.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     buffer - New file buffer.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void setbuf(struct FILE *stream, char *buffer) {
    if (!stream) {
        return;
    }

    fflush(stream);

    if (stream->buffer && !stream->user_buffer) {
        free(stream->buffer);
    }

    if (buffer) {
        stream->user_buffer = 1;
        stream->buffer_type = _IOFBF;
        stream->buffer_size = BUFSIZ;
        stream->buffer_read = 0;
        stream->buffer_pos = 0;
    } else {
        stream->buffer_type = _IONBF;
    }

    stream->buffer = buffer;
}
