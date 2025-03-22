/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/os.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current position along an open FILE. Unlike the normal variant, we
 *     should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     How many bytes are we into the file.
 *-----------------------------------------------------------------------------------------------*/
long ftell_unlocked(FILE *stream) {
    long offset;
    if (__tell_file(stream->handle, &offset)) {
        return -1;
    } else {
        return offset + stream->buffer_pos - stream->buffer_read - stream->unget_size;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current position along an open FILE.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     How many bytes are we into the file.
 *-----------------------------------------------------------------------------------------------*/
long ftell(FILE *stream) {
    flockfile(stream);
    long res = ftell_unlocked(stream);
    funlockfile(stream);
    return res;
}
