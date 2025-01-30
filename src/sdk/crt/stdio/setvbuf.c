/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function overwrites the current file buffer. Differently from setbuf(), this allows
 *     controlling the type of the buffer directly, and the size as well.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     buffer - New file buffer.
 *     mode - Desired buffer type.
 *     size - Buffer size.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void setvbuf(struct FILE *stream, char *buffer, int mode, size_t size) {
    if (!stream) {
        return;
    }

    /* We'll be overwriting those fields in the next block, so we need to save it. */
    bool user_buffer = stream->user_buffer;
    fflush(stream);

    if (mode == _IOLBF || mode == _IOFBF) {
        if (!buffer) {
            buffer = malloc(size);

            if (!buffer) {
                return;
            }

            stream->user_buffer = false;
        } else {
            stream->user_buffer = true;
        }

        stream->buffer_type = mode;
        stream->buffer_size = size;
        stream->buffer_read = 0;
        stream->buffer_pos = 0;
    } else {
        stream->buffer_type = _IONBF;
    }

    if (stream->buffer && !user_buffer) {
        free(stream->buffer);
    }

    stream->buffer = buffer;
}
