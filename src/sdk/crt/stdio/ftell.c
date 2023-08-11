/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current position along an open FILE.
 *
 * PARAMETERS:
 *     stream - Input file stream.
 *
 * RETURN VALUE:
 *     How many bytes are we into the file.
 *-----------------------------------------------------------------------------------------------*/
long ftell(struct FILE *stream) {
    return stream ? stream->file_pos : -1;
}
