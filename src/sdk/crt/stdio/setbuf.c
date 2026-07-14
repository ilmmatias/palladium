/* SPDX-FileCopyrightText: (C) 2023-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function overwrites the current file buffer. It is assumed that the buffer is either
 *     NULL (disable buffering), or at least BUFSIZ bytes longer; If that is not the case, use
 *     setvbuf() instead. Unlike the normal variant, we should only be called after acquiring the
 *     file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     buf - New file buffer.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void setbuf_unlocked(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf) {
    if (!stream) {
        return;
    }

    if (fflush_unlocked(stream) == EOF) {
        return;
    }

    if (stream->buffer && !stream->user_buffer) {
        free(stream->buffer);
    }

    if (buf) {
        stream->user_buffer = true;
        stream->buffer_type = _IOFBF;
        stream->buffer_size = BUFSIZ;
        stream->buffer_read = 0;
        stream->buffer_pos = 0;
    } else {
        stream->user_buffer = false;
        stream->buffer_type = _IONBF;
        stream->buffer_size = 0;
        stream->buffer_read = 0;
        stream->buffer_pos = 0;
    }

    stream->buffer = buf;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function overwrites the current file buffer. It is assumed that the buffer is either
 *     NULL (disable buffering), or at least BUFSIZ bytes longer; If that is not the case, use
 *     setvbuf() instead.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     buf - New file buffer.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void setbuf(FILE *CRT_RESTRICT stream, char *CRT_RESTRICT buf) {
    flockfile(stream);
    setbuf_unlocked(stream, buf);
    funlockfile(stream);
}
