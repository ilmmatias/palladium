/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the ERROR flag is set in the stream.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     `and` between the internal flags and the ERROR flag.
 *-----------------------------------------------------------------------------------------------*/
int ferror(struct FILE *stream) {
    return stream ? stream->flags & __STDIO_FLAGS_ERROR : 0;
}
