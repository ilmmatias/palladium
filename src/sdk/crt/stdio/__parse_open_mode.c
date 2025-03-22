/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/file_flags.h>
#include <stdio.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses the fopen/freopen-style `mode` argument into valid stdio flags.
 *
 * PARAMETERS:
 *     mode - File access mode.
 *
 * RETURN VALUE:
 *     Valid flags for this stdio implementation.
 *-----------------------------------------------------------------------------------------------*/
int __parse_open_mode(const char *mode) {
    int flags = 0;

    /* First character should always be non-empty and be the base type (read/write/append). */
    switch (*mode++) {
        case 'r':
            flags |= __STDIO_FLAGS_READ;
            break;
        case 'w':
            flags |= __STDIO_FLAGS_WRITE;
            break;
        case 'a':
            flags |= __STDIO_FLAGS_WRITE | __STDIO_FLAGS_APPEND;
            break;
    }

    /* Next up, keep on parsing while the string isn't done (the next flags can come up in any
     * order). */
    while (*mode) {
        switch (*mode++) {
            case '+':
                flags |= __STDIO_FLAGS_READ | __STDIO_FLAGS_WRITE;
                break;
            case 'b':
                flags |= __STDIO_FLAGS_BINARY;
                break;
            case 'e':
                flags |= __STDIO_FLAGS_EXEC;
                break;
            case 'x':
                flags |= __STDIO_FLAGS_EXCL;
                break;
        }
    }

    /* No valid flags, we're assuming read-only; Is this correct, or should we just leave the flags
     * empty. */
    return flags ? flags : __STDIO_FLAGS_READ;
}
