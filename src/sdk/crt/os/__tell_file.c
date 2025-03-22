/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function gives out the current position along the given file.
 *
 * PARAMETERS:
 *     handle - OS-specific handle.
 *     pos - Output; Absolute offset from the start of the file.
 *
 * RETURN VALUE:
 *     0 on success, stdio flags describing the error otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __tell_file(void *handle, long *offset) {
    (void)handle;
    (void)offset;
    return __STDIO_FLAGS_ERROR;
}
