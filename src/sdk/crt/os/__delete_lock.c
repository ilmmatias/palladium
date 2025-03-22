/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function tells the OS we're done with the lock, and it can be freed as long as no other
 *     threads are using it.
 *
 * PARAMETERS:
 *     handle - Previously allocated handle to the lock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __delete_lock(void *handle) {
    free(handle);
}
