/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <crt_impl/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries reading `count` chunks of `size` bytes each from the FILE stream. Unlike
 *     the normal variant, we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     ptr - Buffer to store the read data.
 *     size - Size of each element.
 *     count - Amount of elements.
 *     stream - FILE stream; Needs to be open with the read flag.
 *
 * RETURN VALUE:
 *     How many chunks we were able to read without any error.
 *-----------------------------------------------------------------------------------------------*/
size_t fread_unlocked(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    if (!stream || !size || !nmemb) {
        return 0;
    } else if (
        !ptr || !(stream->flags & __STDIO_FLAGS_READ) ||
        (stream->flags & __STDIO_FLAGS_WRITING) || (stream->flags & __STDIO_FLAGS_ERROR) ||
        (stream->flags & __STDIO_FLAGS_EOF)) {
        if (!(stream->flags & __STDIO_FLAGS_EOF)) {
            stream->flags |= __STDIO_FLAGS_ERROR;
        }

        return 0;
    }

    /* Not flushing stdout before reading user input can cause some funky/unexpected behaviour
       (like the prompt not appearing, because it doesn't end with a new line). */
    if (stream == stdin) {
        fflush(stderr);
        fflush(stdout);
    }

    char *dest = ptr;
    stream->flags |= __STDIO_FLAGS_READING;

    size_t total_bytes = size * nmemb;
    size_t accum = 0;
    while (stream->unget_size && total_bytes) {
        *(dest++) = stream->unget_buffer[--stream->unget_size];
        total_bytes--;
        accum++;
    }

    if (!total_bytes) {
        return nmemb;
    }

    if (!stream->buffer || stream->buffer_type == _IONBF) {
        size_t read;
        int flags = __read_file(stream->handle, dest, total_bytes, &read);
        if (flags) {
            stream->flags |= flags;
        }

        return (accum + read) / size;
    }

    /* No support for line buffering on read (_IOLBF == _IOFBF). */
    int flags = 0;
    while (accum < size * nmemb) {
        size_t remaining = size * nmemb - accum;

        if (stream->buffer_pos >= stream->buffer_read) {
            flags = __read_file(
                stream->handle, stream->buffer, stream->buffer_size, &stream->buffer_read);

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

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries reading `count` chunks of `size` bytes each from the FILE stream.
 *
 * PARAMETERS:
 *     buffer - Buffer to store the read data.
 *     size - Size of each element.
 *     nmemb - Amount of elements.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     How many chunks we were able to read without any error.
 *-----------------------------------------------------------------------------------------------*/
size_t fread(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    flockfile(stream);
    size_t res = fread_unlocked(ptr, size, nmemb, stream);
    funlockfile(stream);
    return res;
}
