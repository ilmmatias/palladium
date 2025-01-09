/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns an OS-specific handle after opening a file.
 *
 * PARAMETERS:
 *     filename - Path of the file to be opened; Can be relative.
 *     mode - How we should open this file (read-only, RW, binary, etc).
 *     length - Output; Size in either bytes or characters of the file (based on the mode).
 *
 * RETURN VALUE:
 *     OS-specific handle on success, NULL otherwise.
 *-----------------------------------------------------------------------------------------------*/
void *__fopen(const char *filename, int mode, size_t *length) {
    (void)filename;
    (void)mode;
    (void)length;
    return NULL;
}
