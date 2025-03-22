/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function moves the current position along the file.
 *
 * PARAMETERS:
 *     handle - OS-specific handle.
 *     offset - How far away from the `origin` we want to go.
 *     origin - Which point of the file we want to use as base (SET/start, CUR/current, END/end).
 *
 * RETURN VALUE:
 *     0 on success, stdio flags describing the error otherwise.
 *-----------------------------------------------------------------------------------------------*/
int __seek_file(void *handle, long offset, int origin) {
    (void)handle;
    (void)offset;
    (void)origin;
    return __STDIO_FLAGS_ERROR;
}
