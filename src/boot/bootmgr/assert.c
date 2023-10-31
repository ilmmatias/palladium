/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This functions implements the code to notify the user and halt the system on an
 *     unrecoverable error.
 *
 * PARAMETERS:
 *     Message - What error message to display before halting; It'll be displayed at 0;0 in the
 *               screen.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmPanic(const char *Message) {
    BmSetColor(DISPLAY_COLOR_PANIC);
    BmResetDisplay();
    BmPutString(Message);
    while (1)
        ;
}
