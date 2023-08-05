/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function puts back a character into the `unget` buffer of the file, using it instead
 *     of the file data in the next read operation.
 *
 * PARAMETERS:
 *     ch -  Which character to be put in the buffer.
 *     stream - FILE stream.
 *
 * RETURN VALUE:
 *     The character written into the buffer, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int ungetc(int ch, struct FILE *stream) {
    if (!stream || ch == EOF || stream->unget_size >= sizeof(stream->unget_buffer)) {
        return EOF;
    }

    stream->unget_buffer[stream->unget_size++] = ch;
    stream->flags &= ~__STDIO_FLAGS_EOF;

    return ch;
}
