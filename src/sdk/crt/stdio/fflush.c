/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes all data currently in the buffer back into the file.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     EOF if the operation failed (with the ERROR flag also set), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fflush(struct FILE *stream) {
    /* TODO: We need to flush ALL streams if we're NULL, not error out. */
    if (!stream) {
        return EOF;
    } else if (!(stream->flags & (__STDIO_FLAGS_READING | __STDIO_FLAGS_WRITING))) {
        return 0;
    }

    /* We follow the POSIX standard here; Instead of UB/not doing anything, we reset the input
       buffer position, and drop the unget buffer. */
    if (stream->flags & __STDIO_FLAGS_READING) {
        stream->file_pos += stream->unget_size;
        stream->buffer_file_pos = stream->file_pos;
        stream->buffer_pos = 0;
        stream->buffer_read = 0;
        stream->unget_size = 0;
        return 0;
    } else if (!stream->buffer || stream->buffer_type == _IONBF || !stream->buffer_pos) {
        return 0;
    }

    int flags =
        __fwrite(stream->handle, stream->buffer_file_pos, stream->buffer, stream->buffer_pos, NULL);

    stream->buffer_file_pos += stream->buffer_pos;
    stream->buffer_pos = 0;
    stream->flags |= flags;

    return flags ? EOF : 0;
}
