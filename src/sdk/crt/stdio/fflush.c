/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <crt_impl/os.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes all data currently in the buffer back into the file. Unlike the normal
 *     variant, we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     EOF if the operation failed (with the ERROR flag also set), 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fflush_unlocked(FILE *stream) {
    /* TODO: We need to flush ALL streams if we're NULL, not error out. */
    if (!stream) {
        return EOF;
    } else if (!(stream->flags & (__STDIO_FLAGS_READING | __STDIO_FLAGS_WRITING))) {
        return 0;
    }

    /* We follow the POSIX standard here; Instead of UB/not doing anything, we reset the input
       buffer position, and drop the unget buffer. */
    if (stream->flags & __STDIO_FLAGS_READING) {
        stream->buffer_pos = 0;
        stream->buffer_read = 0;
        stream->unget_size = 0;
        stream->flags &= ~__STDIO_FLAGS_READING;
        return 0;
    } else if (!stream->buffer || stream->buffer_type == _IONBF || !stream->buffer_pos) {
        stream->flags &= ~__STDIO_FLAGS_WRITING;
        return 0;
    }

    size_t wrote;
    int flags = __write_file(stream->handle, stream->buffer, stream->buffer_pos, &wrote);
    stream->buffer_pos = 0;
    stream->flags = (stream->flags | flags) & ~__STDIO_FLAGS_WRITING;

    return flags ? EOF : 0;
}

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
int fflush(FILE *stream) {
    flockfile(stream);
    int res = fflush_unlocked(stream);
    funlockfile(stream);
    return res;
}
