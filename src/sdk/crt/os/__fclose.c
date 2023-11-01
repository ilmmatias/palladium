/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function closes the OS-specific handle, notifying the OS we're done with the file.
 *
 * PARAMETERS:
 *     handle - OS-specific handle.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __fclose(void *handle) {
    (void)handle;
}
