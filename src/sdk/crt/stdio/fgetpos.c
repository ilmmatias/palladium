/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function stores the current position along a FILE stream into an opaque pointer, that
 *     later can be restored using fsetpos.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     pos - Output; Where to save the file position.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fgetpos(struct FILE *stream, struct fpos_t *pos) {
    if (!stream || !pos) {
        return 1;
    }

    pos->file_pos = stream->file_pos;
    return 0;
}
