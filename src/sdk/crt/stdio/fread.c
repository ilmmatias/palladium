/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries reading `count` chunks of `size` bytes each from the FILE stream,
 *
 * PARAMETERS:
 *     buffer - Buffer to store the read data.
 *     size - Size of each element.
 *     count - Amount of elements.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     How many bytes we were able to read without any error.
 *-----------------------------------------------------------------------------------------------*/
size_t fread(void *buffer, size_t size, size_t count, struct FILE *stream) {
    if (!stream || !size || !count) {
        return 0;
    } else if (
        !buffer || !(stream->flags & __STDIO_FLAGS_READ) ||
        (stream->flags & __STDIO_FLAGS_WRITING) || (stream->flags & __STDIO_FLAGS_ERROR) ||
        (stream->flags & __STDIO_FLAGS_EOF)) {
        if (!(stream->flags & __STDIO_FLAGS_EOF)) {
            stream->flags |= __STDIO_FLAGS_ERROR;
        }

        return 0;
    }

    char *dest = buffer;
    stream->flags |= __STDIO_FLAGS_READING;

    /* Element-by-element so that we can break in the exact element we failed. */
    for (size_t i = 0; i < count; i++) {
        int flags = __fread(stream->handle, stream->pos, dest, size);

        if (flags) {
            stream->flags |= flags;
            return i;
        }

        stream->pos += size;
        dest += size;
    }

    return count;
}
