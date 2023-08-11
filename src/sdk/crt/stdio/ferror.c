/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the ERROR flag is set in the stream.
 *
 * PARAMETERS:
 *     stream - Input file stream.
 *
 * RETURN VALUE:
 *     `and` between the internal flags and the ERROR flag.
 *-----------------------------------------------------------------------------------------------*/
int ferror(struct FILE *stream) {
    return stream ? stream->flags & __STDIO_FLAGS_ERROR : 0;
}
