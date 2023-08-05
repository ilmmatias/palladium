/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fread(&ch, 1, 1, stream), returning either `ch`, or EOF on failure.
 *
 * PARAMETERS:
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_READ.
 *
 * RETURN VALUE:
 *     Data stored in the fread buffer, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fgetc(FILE *stream) {
    int ch;
    return fread(&ch, 1, 1, stream) ? ch : EOF;
}
