/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/os.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function locks the file for access by only the current thread. We always acquire the
 *     lock, blocking the thread if required (until the lock is freed for us to acquire it).
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void flockfile(FILE *stream) {
    if (stream) {
        __acquire_lock(stream->lock_handle);
    }
}
