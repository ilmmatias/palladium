/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/os.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function unlocks the file for access by other threads. You should only call this
 *     function if the current thread has acquired the lock, or the host OS reserves the right to
 *     terminate the program with an error.
 *
 * PARAMETERS:
 *     stream - Pointer to an open file handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void funlockfile(FILE *stream) {
    if (stream) {
        __release_lock(stream->lock_handle);
    }
}
