/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function fills in the input buffer using the file.
 *
 * PARAMETERS:
 *     handle - OS-specific handle.
 *     buffer - Output buffer to fill up.
 *     size - How many bytes/characters we want to read.
 *     read - Output; How many bytes/characters we wrote into the buffer.
 *
 * RETURN VALUE:
 *     0 on success, stdio flags describing the error otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __read_file(void *handle, void *buffer, size_t size, size_t *read) {
    (void)handle;
    (void)buffer;
    (void)size;
    (void)read;
    return __STDIO_FLAGS_ERROR;
}
