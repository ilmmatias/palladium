/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function repeatedly reads bytes from stdin until we reach EOF or a new line. Unlike the
 *     normal variant, we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     str - Output; Where to store the read data.
 *
 * RETURN VALUE:
 *     Pointer to the start of the output, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
char *gets_unlocked(char *str) {
    FILE *stream = stdin;

    if ((stream->flags & __STDIO_FLAGS_ERROR) || (stream->flags & __STDIO_FLAGS_EOF)) {
        if (!(stream->flags & __STDIO_FLAGS_EOF)) {
            stream->flags |= __STDIO_FLAGS_ERROR;
        }

        return NULL;
    }

    bool fail = false;
    char *dest = str;
    stream->flags |= __STDIO_FLAGS_READING;

    while (true) {
        int ch = fgetc_unlocked(stream);
        if (ch == EOF) {
            fail = true;
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
    flockfile(stdin);
    char *res = gets_unlocked(str);
    funlockfile(stdin);
    return res;
}
