/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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
void __close_file(void *handle) {
    (void)handle;
}
