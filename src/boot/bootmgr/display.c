/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs the specified character buffer into the screen.
 *
 * PARAMETERS:
 *     buffer - What to output.
 *     size - Size of the buffer.
 *     context - Ignored by us, can be NULL or whatever you want.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __put_stdout(const void *buffer, int size, void *context) {
    (void)context;
    const char *src = buffer;
    while (size--) {
        BiPutChar(*(src++));
    }
}
