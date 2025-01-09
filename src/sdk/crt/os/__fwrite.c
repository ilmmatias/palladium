/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes the data from the input buffer into the file.
 *
 * PARAMETERS:
 *     handle - OS-specific handle.
 *     pos - Absolute offset from the start of the file.
 *     buffer - Input buffer; What we should write into the file.
 *     size - How many bytes/characters we want to write.
 *     read - Output; How many bytes/characters we wrote into the file.
 *
 * RETURN VALUE:
 *     0 on success, stdio flags describing the error otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __fwrite(void *handle, size_t pos, const void *buffer, size_t size, size_t *wrote) {
    (void)handle;
    (void)pos;
    (void)buffer;
    (void)size;
    (void)wrote;
    return __STDIO_FLAGS_ERROR;
}
