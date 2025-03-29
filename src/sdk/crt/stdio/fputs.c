/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fwrite_unlocked(str, 1, strlen(str), stream).
 *
 * PARAMETERS:
 *     s - What to write into the file.
 *     stream - FILE stream; Needs to be open with the write flag.
 *
 * RETURN VALUE:
 *     Data written onto the file, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fputs_unlocked(const char *CRT_RESTRICT s, FILE *CRT_RESTRICT stream) {
    size_t count = strlen(s);
    return fwrite_unlocked((void *)s, 1, count, stream) == count ? count : EOF;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes a string into the file stream.
 *
 * PARAMETERS:
 *     str - What to write into the file.
 *     stream - FILE stream; Needs to be open with the write flag.
 *
 * RETURN VALUE:
 *     Size of the data written onto the file, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fputs(const char *CRT_RESTRICT s, FILE *CRT_RESTRICT stream) {
    flockfile(stream);
    int res = fputs_unlocked(s, stream);
    funlockfile(stream);
    return res;
}
