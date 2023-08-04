/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
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
FILE *freopen(const char *filename, const char *mode, struct FILE *stream) {
    if (!stream) {
        return NULL;
    }

    __fclose(stream->handle);

    if (!filename || !mode) {
        free(stream);
        return NULL;
    }

    int flags = __parse_fopen_mode(mode);
    void *handle = __fopen(filename, flags);
    if (!handle) {
        free(stream);
        return NULL;
    }

    stream->handle = handle;
    stream->flags = flags;
    stream->pos = 0;

    return stream;
}
