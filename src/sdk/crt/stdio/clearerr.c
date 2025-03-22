/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets the file flags back to a no error/EOF state. Unlike the normal variant,
 *     we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
void clearerr_unlocked(FILE *stream) {
    if (stream) {
        stream->flags &= ~(__STDIO_FLAGS_EOF | __STDIO_FLAGS_ERROR);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets the file flags back to a no error/EOF state.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
void clearerr(FILE *stream) {
    flockfile(stream);
    clearerr_unlocked(stream);
    funlockfile(stream);
}
