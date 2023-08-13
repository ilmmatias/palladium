/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "/home/italo/Palladium/src/boot/bootmgr/include/display.h"

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
size_t fwrite(const void *buffer, size_t size, size_t count, struct FILE *stream) {
    if (!stream || !size || !count) {
        return 0;
    } else if (
        !buffer || !(stream->flags & __STDIO_FLAGS_WRITE) ||
        (stream->flags & __STDIO_FLAGS_READING) || (stream->flags & __STDIO_FLAGS_ERROR)) {
        stream->flags |= __STDIO_FLAGS_ERROR;
        return 0;
    }

    const char *src = buffer;
    stream->flags |= __STDIO_FLAGS_WRITING;

    size_t wrote;
    size_t total_bytes = size * count;

    if (!stream->buffer || stream->buffer_type == _IONBF) {
        int flags = __fwrite(stream->handle, stream->file_pos, src, total_bytes, &wrote);

        stream->file_pos += (wrote / size) * size;
        if (flags) {
            stream->flags |= flags;
        }

        return wrote / size;
    }

    int new_line = 0;
    size_t accum = 0;

    /* The procedure for _IOFBF and _IOLBF are somewhat simillar; Check where the next new line
       is (takes into consideration the buffer size); Write all that we can into the buffer,
       flushing if we reached the end of the buffer or if we have a newline on _IOLBF. */
    while (accum < total_bytes) {
        size_t remaining = size * count - accum;
        size_t copy_size = stream->buffer_size - stream->buffer_pos;
        if (remaining < copy_size) {
            copy_size = remaining;
        }

        if (stream->buffer_type == _IOLBF) {
            char *pos = memchr(src, '\n', copy_size);
            if (pos) {
                copy_size = pos - src + 1;
                new_line = 1;
            }
        }

        memcpy(stream->buffer + stream->buffer_pos, src, copy_size);
        stream->file_pos += copy_size;
        stream->buffer_pos += copy_size;
        accum += copy_size;
        src += copy_size;

        if (new_line || stream->buffer_pos >= stream->buffer_size) {
            int flags = __fwrite(
                stream->handle,
                stream->buffer_file_pos,
                stream->buffer,
                stream->buffer_pos,
                &wrote);

            if (wrote < stream->buffer_pos) {
                memmove(stream->buffer, stream->buffer + wrote, stream->buffer_pos - wrote);
            }

            stream->buffer_file_pos += wrote;
            stream->buffer_pos -= wrote;

            if (flags) {
                stream->flags |= flags;
                break;
            }
        }
    }

    return accum / size;
}
