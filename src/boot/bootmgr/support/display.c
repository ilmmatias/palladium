/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs the specified character buffer into the screen.
 *
 * PARAMETERS:
 *     String - What to output.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmPutString(const char *String) {
    while (*String) {
        BmPutChar(*(String++));
    }
}
