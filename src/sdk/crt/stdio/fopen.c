/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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

    int flags = __parse_fopen_mode(mode);
    void *handle = __fopen(filename, flags);
    if (!handle) {
        return NULL;
    }

    /* Why struct vs no struct? Just to make vscode/intellisense happy. */
    struct FILE *stream = malloc(sizeof(FILE));
    if (!stream) {
        __fclose(handle);
        return NULL;
    }

    stream->handle = handle;
    stream->flags = flags;
    stream->pos = 0;

    return stream;
}
