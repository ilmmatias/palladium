/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries reading `count` chunks of `size` bytes each from the FILE stream.
 *
 * PARAMETERS:
 *     buffer - Buffer to store the read data.
 *     size - Size of each element.
 *     count - Amount of elements.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     How many chunks we were able to read without any error.
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

    size_t total_bytes = size * count;
    size_t accum = 0;
    while (stream->unget_size && total_bytes) {
        *(dest++) = stream->unget_buffer[--stream->unget_size];
        stream->file_pos++;
        total_bytes--;
        accum++;
    }

    if (!total_bytes) {
        return count;
    }

    if (!stream->buffer || stream->buffer_type == _IONBF) {
        size_t read;
        int flags = __fread(stream->handle, stream->file_pos, dest, total_bytes, &read);

        stream->file_pos += (read / size) * size;
        if (flags) {
            stream->flags |= flags;
        }

        return (accum + read) / size;
    }

    /* No support for line buffering on read (_IOLBF == _IOFBF). */
    int flags = 0;
    while (accum < size * count) {
        size_t remaining = size * count - accum;

        if (stream->buffer_pos >= stream->buffer_read) {
            flags = __fread(
                stream->handle,
                stream->buffer_file_pos,
                stream->buffer,
                stream->buffer_size,
                &stream->buffer_read);

            stream->buffer_file_pos += stream->buffer_read;
            stream->buffer_pos = 0;

            /* EOF is only valid/set if the user actually tried reading beyond the file
               boundaries. */
            if ((flags & __STDIO_FLAGS_EOF) && remaining <= stream->buffer_read) {
                flags &= ~__STDIO_FLAGS_EOF;
            }
        }

        size_t copy_size = stream->buffer_read - stream->buffer_pos;
        if (remaining < copy_size) {
            copy_size = remaining;
        }

        memcpy(dest, stream->buffer + stream->buffer_pos, copy_size);
        stream->file_pos += copy_size;
        stream->buffer_pos += copy_size;
        accum += copy_size;
        dest += copy_size;

        if (flags) {
            stream->flags |= flags;
            break;
        }
    }

    return accum / size;
}
