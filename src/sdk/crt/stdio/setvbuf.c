/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function overwrites the current file buffer. Differently from setbuf(), this allows
 *     controlling the type of the buffer directly, and the size as well. Unlike the normal variant,
 *     we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     buf - New file buffer.
 *     mode - Desired buffer type.
 *     size - Buffer size.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int setvbuf_unlocked(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf, int mode, size_t size) {
    if (!stream || (mode != _IONBF && mode != _IOLBF && mode != _IOFBF)) {
        return 1;
    }

    /* We'll be overwriting those fields in the next block, so we need to save it. */
    bool user_buffer = stream->user_buffer;
    if (fflush_unlocked(stream) == EOF) {
        return 1;
    }

    if (mode == _IOLBF || mode == _IOFBF) {
        if (!size) {
            return 1;
        }

        if (!buf) {
            buf = malloc(size);

            if (!buf) {
                return 1;
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
        stream->user_buffer = false;
        stream->buffer_type = _IONBF;
        stream->buffer_size = 0;
        stream->buffer_read = 0;
        stream->buffer_pos = 0;
    }

    if (stream->buffer && !user_buffer) {
        free(stream->buffer);
    }

    stream->buffer = buf;
    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function overwrites the current file buffer. Differently from setbuf(), this allows
 *     controlling the type of the buffer directly, and the size as well.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     buf - New file buffer.
 *     mode - Desired buffer type.
 *     size - Buffer size.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int setvbuf(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf, int mode, size_t size) {
    flockfile(stream);
    int res = setvbuf_unlocked(stream, buf, mode, size);
    funlockfile(stream);
    return res;
}
