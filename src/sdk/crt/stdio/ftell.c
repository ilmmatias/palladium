/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current position along an open FILE.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     How many bytes are we into the file.
 *-----------------------------------------------------------------------------------------------*/
long ftell(struct FILE *stream) {
    return stream ? stream->file_pos : -1;
}
