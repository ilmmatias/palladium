/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function repeatedly reads the file stream until we reach EOF, a new line, or the
 *     exceed/reach the max `count`, minus 1 for the null terminator. Unlike the normal variant, we
 *     should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     s - Destination buffer.
 *     count - Max amount of bytes to read into the buffer.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     The destination buffer itself on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
char *fgets_unlocked(char *CRT_RESTRICT s, int count, FILE *CRT_RESTRICT stream) {
    if (!stream) {
        return NULL;
    } else if (
        !s || !count || !(stream->flags & __STDIO_FLAGS_READ) ||
        (stream->flags & __STDIO_FLAGS_WRITING) || (stream->flags & __STDIO_FLAGS_ERROR) ||
        (stream->flags & __STDIO_FLAGS_EOF)) {
        if (!(stream->flags & __STDIO_FLAGS_EOF)) {
            stream->flags |= __STDIO_FLAGS_ERROR;
        }

        return NULL;
    }

    bool fail = false;
    char *dest = s;
    stream->flags |= __STDIO_FLAGS_READING;

    while (count > 1) {
        int ch = fgetc_unlocked(stream);

        if (ch == EOF) {
            fail = true;
            break;
        }

        *(dest++) = ch;
        count--;

        if (ch == '\n') {
            break;
        }
    }

    *dest = 0;
    return fail ? NULL : s;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function repeatedly reads bytes until we reach EOF, a new line, or the exceed/reach
 *     the max `count`, minus 1 for the null terminator.
 *
 * PARAMETERS:
 *     s - Destination buffer.
 *     count - Max amount of bytes to read into the buffer.
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     The destination buffer itself on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
char *fgets(char *CRT_RESTRICT s, int count, FILE *CRT_RESTRICT stream) {
    flockfile(stream);
    char *res = fgets_unlocked(s, count, stream);
    funlockfile(stream);
    return res;
}
