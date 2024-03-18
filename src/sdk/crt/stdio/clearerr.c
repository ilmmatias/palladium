/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>

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
void clearerr(struct FILE *stream) {
    if (stream) {
        stream->flags &= ~(__STDIO_FLAGS_EOF | __STDIO_FLAGS_ERROR);
    }
}
