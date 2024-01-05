/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
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
FILE *fopen(const char *filename, const char *mode) {
    if (!filename || !mode) {
        return NULL;
    }

    size_t length;
    int flags = __parse_fopen_mode(mode);
    void *handle = __fopen(filename, flags, &length);
    if (!handle) {
        return NULL;
    }

    /* Why struct vs no struct? Just to make vscode/intellisense happy. */
    struct FILE *stream = malloc(sizeof(FILE));
    if (!stream) {
        __fclose(handle);
        return NULL;
    }

    /* By default we make all files fully buffered. */
    stream->buffer = malloc(BUFSIZ);
    if (!stream->buffer) {
        free(stream);
        __fclose(handle);
        return NULL;
    }

    stream->handle = handle;
    stream->file_size = length;
    stream->file_pos = 0;
    stream->buffer_type = _IOFBF;
    stream->buffer_size = BUFSIZ;
    stream->buffer_read = 0;
    stream->buffer_pos = 0;
    stream->buffer_file_pos = 0;
    stream->unget_size = 0;
    stream->flags = flags;

    return stream;
}
