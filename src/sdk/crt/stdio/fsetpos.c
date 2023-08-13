/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores a previously FILE position obtained using `fgetpos`.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     pos - Position obtained from `fgetpos`.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fsetpos(struct FILE *stream, struct fpos_t *pos) {
    if (!stream || !pos) {
        return 1;
    }

    fflush(stream);
    stream->flags &= ~__STDIO_FLAGS_EOF;
    stream->file_pos = pos->file_pos;
    stream->buffer_file_pos = pos->file_pos;

    return 0;
}
