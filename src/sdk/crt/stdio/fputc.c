/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around fwrite(&ch, 1, 1, stream), returning either `ch`, or EOF on failure.
 *
 * PARAMETERS:
 *     stream - FILE stream; Needs to be open as __STDIO_FLAGS_WRITE.
 *
 * RETURN VALUE:
 *     Data written onto the file, or EOF on failure.
 *-----------------------------------------------------------------------------------------------*/
int fputc(int ch, FILE *stream) {
    return fwrite(&ch, 1, 1, stream) ? ch : EOF;
}
