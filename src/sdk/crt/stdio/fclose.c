/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/os.h>
#include <stdio.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes an existing handle, freeing the FILE husk in the process.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     0 on success, EOF otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fclose(FILE *stream) {
    if (!stream) {
        return EOF;
    }

    fflush_unlocked(stream);
    __close_file(stream->handle);

    if (stream->user_lock) {
        __delete_lock(stream->lock_handle);
    }

    if (stream->buffer && !stream->user_buffer) {
        free(stream->buffer);
    }

    free(stream);

    return 0;
}
