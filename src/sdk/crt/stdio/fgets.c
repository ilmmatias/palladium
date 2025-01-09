/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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

    int fail = 0;
    char *dest = str;
    stream->flags |= __STDIO_FLAGS_READING;

    while (count > 1) {
        int ch = fgetc(stream);

        if (ch == EOF) {
            fail = 1;
            break;
        }

        *(dest++) = ch;
        count--;

        if (ch == '\n') {
            break;
        }
    }

    *dest = 0;
    return fail ? NULL : str;
}
