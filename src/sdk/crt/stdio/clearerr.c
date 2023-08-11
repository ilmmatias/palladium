/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets the file flags back to a no error/EOF state.
 *
 * PARAMETERS:
 *     stream - Input file stream.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
void clearerr(struct FILE *stream) {
    if (stream) {
        stream->flags &= ~(__STDIO_FLAGS_EOF | __STDIO_FLAGS_ERROR);
    }
}
