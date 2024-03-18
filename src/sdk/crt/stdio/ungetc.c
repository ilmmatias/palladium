/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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
 *     stream - Pointer to an open file handle.
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
    stream->file_pos--;

    return ch;
}
