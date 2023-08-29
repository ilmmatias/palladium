/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function displays a NULL terminated string to the screen, by calling VidPutChar() for
 *     each character in it.
 *
 * PARAMETERS:
 *     String - Data to be displayed.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPutString(const char *String) {
    if (String) {
        while (*String) {
            VidPutChar(*(String++));
        }
    }
}
