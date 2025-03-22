/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the given lock. This function either instantly acquires the lock, or
 *     asks the OS to schedule out the current thread while waiting for the lock to be released.
 *
 * PARAMETERS:
 *     handle - Previously allocated handle to the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __acquire_lock(void *handle) {
    (void)handle;
}
