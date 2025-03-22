/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <crt_impl/os.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function updates the current FILE position based on the specified `offset` from the
 *     `origin` point within the stream. Unlike the normal variant, we should only be called after
 *     acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     offset - How far away from the `origin` we want to go.
 *     whence - Which point of the file we want to use as base (SET/start, CUR/current, END/end).
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fseek_unlocked(FILE *stream, long int offset, int whence) {
    if (!stream) {
        return 1;
    }

    fflush_unlocked(stream);

    /* Assume that as long as seek_file doesn't fail, the result of fflush doesn't matter. */
    int flags = __seek_file(stream->handle, offset, whence);
    if (flags) {
        stream->flags |= flags;
    } else {
        stream->flags &= ~__STDIO_FLAGS_EOF;
    }

    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function updates the current FILE position based on the specified `offset` from the
 *     `origin` point within the stream.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     offset - How far away from the `origin` we want to go.
 *     whence - Which point of the file we want to use as base (SET/start, CUR/current, END/end).
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fseek(FILE *stream, long int offset, int whence) {
    flockfile(stream);
    int res = fseek_unlocked(stream, offset, whence);
    funlockfile(stream);
    return res;
}
