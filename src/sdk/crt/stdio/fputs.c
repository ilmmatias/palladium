/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fwrite(str, 1, strlen(str), stream).
 *
 * PARAMETERS:
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_WRITE.
 *
 * RETURN VALUE:
 *     EOF on failure, size of the string otherwise.
 *-----------------------------------------------------------------------------------------------*/
int fputs(const char *str, FILE *stream) {
    size_t count = strlen(str);
    return fwrite((void *)str, 1, count, stream) == count ? count : EOF;
}
