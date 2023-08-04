/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes an existing handle, freeing the FILE husk in the process.
 *
 * PARAMETERS:
 *     stream - FILE husk to be closed.
 *
 * RETURN VALUE:
 *     0 on success, EOF otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fclose(struct FILE *stream) {
    if (stream) {
        __fclose(stream->handle);
        return 0;
    } else {
        return EOF;
    }
}
