/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function repeatedly reads bytes until we reach EOF, a new line, or the exceed/reach
 *     the max `count`, minus 1 for the null terminator.
 *
 * PARAMETERS:
 *     str - Destination buffer.
 *     count - Max amount of bytes to read into the buffer.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     The destination buffer itself on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
char *fgets(char *str, int count, struct FILE *stream) {
    if (!stream) {
        return NULL;
    } else if (
        !str || !count || !(stream->flags & __STDIO_FLAGS_READ) ||
        (stream->flags & __STDIO_FLAGS_WRITING) || (stream->flags & __STDIO_FLAGS_ERROR) ||
        (stream->flags & __STDIO_FLAGS_EOF)) {
        if (!(stream->flags & __STDIO_FLAGS_EOF)) {
            stream->flags |= __STDIO_FLAGS_ERROR;
        }

        return NULL;
    }

    int any = 0;
    char *dest = str;
    stream->flags |= __STDIO_FLAGS_READING;

    while (count > 1) {
        int ch = fgetc(stream);

        if (ch == EOF) {
            break;
        }

        *(dest++) = ch;
        count--;
        any = 1;

        if (ch == '\n') {
            break;
        }
    }

    if (any) {
        *dest = 0;
        return str;
    } else {
        return NULL;
    }
}
