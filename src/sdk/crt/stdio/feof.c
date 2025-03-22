/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the EOF flag is set in the stream. Unlike the normal variant,
 *     we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     A non-zero value if the EOF flag is set, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int feof_unlocked(FILE *stream) {
    return stream ? stream->flags & __STDIO_FLAGS_EOF : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the EOF flag is set in the stream.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     A non-zero value if the EOF flag is set, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int feof(FILE *stream) {
    flockfile(stream);
    int res = feof_unlocked(stream);
    funlockfile(stream);
    return res;
}
