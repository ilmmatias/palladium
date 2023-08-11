/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores a previously FILE position obtained using `fgetpos`.
 *
 * PARAMETERS:
 *     stream - Input file stream.
 *     pos - Position obtained from `fgetpos`.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fsetpos(struct FILE *stream, struct fpos_t *pos) {
    if (!stream) {
        return 1;
    }

    stream->flags &= ~__STDIO_FLAGS_EOF;
    stream->unget_size = 0;
    stream->file_pos = pos->file_pos;
    stream->buffer_file_pos = pos->file_pos;
    stream->buffer_pos = 0;

    return 0;
}
