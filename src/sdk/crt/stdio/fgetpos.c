/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function stores the current position along a FILE stream into an opaque pointer, that
 *     later can be restored using fsetpos. Unlike the normal variant, we should only be called
 *     after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     pos - Output; Where to save the file position.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fgetpos_unlocked(FILE *restrict stream, fpos_t *restrict pos) {
    /* I'm not sure if this is the actual right way to implement this? */
    long offset = ftell_unlocked(stream);
    if (offset != -1) {
        *pos = offset;
        return 0;
    } else {
        return 1;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function stores the current position along a FILE stream into an opaque pointer, that
 *     later can be restored using fsetpos.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     pos - Output; Where to save the file position.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fgetpos(FILE *restrict stream, fpos_t *restrict pos) {
    flockfile(stream);
    int res = fgetpos_unlocked(stream, pos);
    funlockfile(stream);
    return res;
}
