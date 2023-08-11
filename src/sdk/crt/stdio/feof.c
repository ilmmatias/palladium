/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the EOF flag is set in the stream.
 *
 * PARAMETERS:
 *     stream - Input file stream.
 *
 * RETURN VALUE:
 *     `and` between the internal flags and the EOF flag.
 *-----------------------------------------------------------------------------------------------*/
int feof(struct FILE *stream) {
    return stream ? stream->flags & __STDIO_FLAGS_EOF : 0;
}
