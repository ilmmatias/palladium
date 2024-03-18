/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#define __CRT_IMPL
#include <crt_impl.h>
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
int __parse_fopen_mode(const char *mode) {
    if (!strcmp(mode, "r+") || !strcmp(mode, "rb+") || !strcmp(mode, "r+b") ||
        !strcmp(mode, "w+") || !strcmp(mode, "wb+") || !strcmp(mode, "w+b")) {
        return __STDIO_FLAGS_READ | __STDIO_FLAGS_WRITE | __STDIO_FLAGS_UPDATE;
    } else if (
        !strcmp(mode, "r+x") || !strcmp(mode, "rb+x") || !strcmp(mode, "r+bx") ||
        !strcmp(mode, "w+x") || !strcmp(mode, "wb+x") || !strcmp(mode, "w+bx")) {
        return __STDIO_FLAGS_READ | __STDIO_FLAGS_WRITE | __STDIO_FLAGS_UPDATE |
               __STDIO_FLAGS_NO_OVERWRITE;
    } else if (!strcmp(mode, "w") || !strcmp(mode, "wb")) {
        return __STDIO_FLAGS_WRITE;
    } else if (!strcmp(mode, "wx") || !strcmp(mode, "wbx")) {
        return __STDIO_FLAGS_WRITE | __STDIO_FLAGS_NO_OVERWRITE;
    } else if (!strcmp(mode, "a") || !strcmp(mode, "ab")) {
        return __STDIO_FLAGS_WRITE | __STDIO_FLAGS_APPEND;
    } else if (!strcmp(mode, "a+") || !strcmp(mode, "ab+") || !strcmp(mode, "a+b")) {
        return __STDIO_FLAGS_WRITE | __STDIO_FLAGS_APPEND | __STDIO_FLAGS_UPDATE;
    } else {
        return __STDIO_FLAGS_READ;
    }
}
