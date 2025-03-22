/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <crt_impl/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes an existing handle, overwriting it with a new one.
 *
 * PARAMETERS:
 *     filename - Path of the file to be requested.
 *     mode - File access mode.
 *     stream - FILE pointer containing the handle to overwrite.
 *
 * RETURN VALUE:
 *     `stream` if the new handle has been opened properly, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
FILE *freopen(const char *restrict filename, const char *restrict mode, FILE *restrict stream) {
    if (!stream) {
        return NULL;
    }

    flockfile(stream);
    fflush_unlocked(stream);
    __close_file(stream->handle);

    if (stream->buffer && !stream->user_buffer) {
        free(stream->buffer);
    }

    if (!filename || !mode) {
        if (stream->user_lock) {
            __delete_lock(stream->lock_handle);
        }

        free(stream);
        return NULL;
    }

    int flags = __parse_open_mode(mode);
    void *handle = __open_file(filename, flags);
    if (!handle) {
        if (stream->user_lock) {
            __delete_lock(stream->lock_handle);
        }

        free(stream);
        return NULL;
    }

    stream->buffer = malloc(BUFSIZ);
    if (!stream->buffer) {
        if (stream->user_lock) {
            __delete_lock(stream->lock_handle);
        }

        __close_file(handle);
        free(stream);
        return NULL;
    }

    stream->handle = handle;
    stream->buffer_type = _IOFBF;
    stream->buffer_size = BUFSIZ;
    stream->buffer_read = 0;
    stream->buffer_pos = 0;
    stream->unget_size = 0;
    stream->flags = flags;

    funlockfile(stream);
    return stream;
}
