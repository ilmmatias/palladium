/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores a previously FILE position obtained using `fgetpos`. Unlike the normal
 *     variant, we should only be called after acquiring the file lock.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     pos - Position obtained from `fgetpos`.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fsetpos_unlocked(FILE *stream, const fpos_t *pos) {
    /* I'm not sure if this is the actual right way to implement this? */
    return fseek_unlocked(stream, *pos, SEEK_SET);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function restores a previously FILE position obtained using `fgetpos`.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *     pos - Position obtained from `fgetpos`.
 *
 * RETURN VALUE:
 *     0 on success, 1 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fsetpos(FILE *stream, const fpos_t *pos) {
    flockfile(stream);
    int res = fsetpos_unlocked(stream, pos);
    funlockfile(stream);
    return res;
}
