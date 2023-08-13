/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function repeatedly reads bytes from stdin until we reach EOF or a new line.
 *
 * PARAMETERS:
 *     str - Output; Where to store the read data.
 *
 * RETURN VALUE:
 *     Pointer to the start of the output, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
char *gets(char *str) {
    struct FILE *stream = stdin;

    if ((stream->flags & __STDIO_FLAGS_ERROR) || (stream->flags & __STDIO_FLAGS_EOF)) {
        if (!(stream->flags & __STDIO_FLAGS_EOF)) {
            stream->flags |= __STDIO_FLAGS_ERROR;
        }

        return NULL;
    }

    int fail = 0;
    char *dest = str;
    stream->flags |= __STDIO_FLAGS_READING;

    while (1) {
        int ch = fgetc(stream);
        if (ch == EOF) {
            fail = 1;
            break;
        }

        *(dest++) = ch;
        if (ch == '\n') {
            break;
        }
    }

    *dest = 0;
    return fail ? NULL : str;
}
