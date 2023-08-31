/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <vid.h>

static char *Messages[] = {
    "FATAL_ERROR",
    "CORRUPTED_HARDWARE_STRUCTURES",
};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function takes over the display, and writes a fatal error message (panic), followed by
 *     halting the system.
 *
 * PARAMETERS:
 *     Message - Number of the error code/message to be shown.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KeFatalError(int Message) {
    if (Message < KE_FATAL_ERROR || Message > KE_CORRUPTED_HARDWARE_STRUCTURES) {
        Message = KE_FATAL_ERROR;
    }

    VidSetColor(VID_COLOR_PANIC);
    VidResetDisplay();

    VidPutString("A fatal error has occoured, and the system cannot safely recover operation.\n");
    VidPutString("You'll need to reboot your computer.\n");
    VidPutString("Error Code: ");
    VidPutString(Messages[Message]);

    while (1)
        ;
}
