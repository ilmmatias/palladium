/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the given lock, signaling other waiting threads that they can acquire
 *     it.
 *
 * PARAMETERS:
 *     handle - Previously allocated handle to the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __release_lock(void *handle) {
    (void)handle;
}
