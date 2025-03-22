/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <crt_impl/os.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries writing `count` chunks of `size` bytes each into the FILE stream. Unlike
 *     the normal variant, we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     ptr - Buffer to read the data from.
 *     size - Size of each element.
 *     nmemb - Amount of elements.
 *     stream - FILE stream; Needs to be open with the write flag.
 *
 * RETURN VALUE:
 *     How many chunks we were able to write without any error.
 *-----------------------------------------------------------------------------------------------*/
size_t fwrite_unlocked(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    if (!stream || !size || !nmemb) {
        return 0;
    } else if (
        !ptr || !(stream->flags & __STDIO_FLAGS_WRITE) || (stream->flags & __STDIO_FLAGS_READING) ||
        (stream->flags & __STDIO_FLAGS_ERROR)) {
        stream->flags |= __STDIO_FLAGS_ERROR;
        return 0;
    }

    const char *src = ptr;
    stream->flags |= __STDIO_FLAGS_WRITING;

    size_t wrote;
    size_t total_bytes = size * nmemb;

    if (!stream->buffer || stream->buffer_type == _IONBF) {
        int flags = __write_file(stream->handle, src, total_bytes, &wrote);
        if (flags) {
            stream->flags |= flags;
        }

        return wrote / size;
    }

    bool new_line = false;
    size_t accum = 0;

    /* The procedure for _IOFBF and _IOLBF are somewhat simillar; Check where the next new line
       is (takes into consideration the buffer size); Write all that we can into the buffer,
       flushing if we reached the end of the buffer or if we have a newline on _IOLBF. */
    while (accum < total_bytes) {
        size_t remaining = size * nmemb - accum;
        size_t copy_size = stream->buffer_size - stream->buffer_pos;
        if (remaining < copy_size) {
            copy_size = remaining;
        }

        if (stream->buffer_type == _IOLBF) {
            char *pos = memchr(src, '\n', copy_size);
            if (pos) {
                copy_size = pos - src + 1;
                new_line = true;
            }
        }

        memcpy(stream->buffer + stream->buffer_pos, src, copy_size);
        stream->buffer_pos += copy_size;
        accum += copy_size;
        src += copy_size;

        if (new_line || stream->buffer_pos >= stream->buffer_size) {
            int flags = __write_file(stream->handle, stream->buffer, stream->buffer_pos, &wrote);

            if (wrote < stream->buffer_pos) {
                memmove(stream->buffer, stream->buffer + wrote, stream->buffer_pos - wrote);
            }

            stream->buffer_pos -= wrote;

            if (flags) {
                stream->flags |= flags;
                break;
            }
        }
    }

    return accum / size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tries writing `count` chunks of `size` bytes each into the FILE stream,
 *
 * PARAMETERS:
 *     buffer - Buffer to read the data from.
 *     size - Size of each element.
 *     count - Amount of elements.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_WRITE.
 *
 * RETURN VALUE:
 *     How many chunks we were able to write without any error.
 *-----------------------------------------------------------------------------------------------*/
size_t fwrite(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    flockfile(stream);
    size_t res = fwrite_unlocked(ptr, size, nmemb, stream);
    funlockfile(stream);
    return res;
}
