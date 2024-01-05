/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
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
int fclose(struct FILE *stream) {
    if (!stream) {
        return EOF;
    }

    fflush(stream);
    __fclose(stream->handle);

    if (stream->buffer && !stream->user_buffer) {
        free(stream->buffer);
    }

    free(stream);

    return 0;
}
