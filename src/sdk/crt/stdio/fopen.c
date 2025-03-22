/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <crt_impl/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function requests the OS for a file access handle, passing along the access flags.
 *
 * PARAMETERS:
 *     filename - Path of the file to be requested.
 *     mode - File access mode.
 *
 * RETURN VALUE:
 *     FILE containing the OS-specific handle and the CRT data, or NULL if something failed
 *     along the way.
 *-----------------------------------------------------------------------------------------------*/
FILE *fopen(const char *restrict filename, const char *restrict mode) {
    if (!filename || !mode) {
        return NULL;
    }

    int flags = __parse_open_mode(mode);
    void *handle = __open_file(filename, flags);
    if (!handle) {
        return NULL;
    }

    void *lock_handle = __create_lock();
    if (!lock_handle) {
        __close_file(handle);
        return NULL;
    }

    FILE *stream = malloc(sizeof(FILE));
    if (!stream) {
        __delete_lock(lock_handle);
        __close_file(handle);
        return NULL;
    }

    /* By default we make all files fully buffered. */
    stream->buffer = malloc(BUFSIZ);
    if (!stream->buffer) {
        free(stream);
        __delete_lock(lock_handle);
        __close_file(handle);
        return NULL;
    }

    stream->handle = handle;
    stream->lock_handle = lock_handle;
    stream->user_lock = true;
    stream->buffer_type = _IOFBF;
    stream->buffer_size = BUFSIZ;
    stream->buffer_read = 0;
    stream->buffer_pos = 0;
    stream->unget_size = 0;
    stream->flags = flags;

    return stream;
}
