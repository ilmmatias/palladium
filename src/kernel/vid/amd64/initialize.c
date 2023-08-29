/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>
#include <vid.h>

uint16_t *VidpSurface = (uint16_t *)0xFFFF8000000B8000;
int VidpAttribute = 0x07;
int VidpCursorX = 0;
int VidpCursorY = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the display after initial system bootup (taking over whatever the
 *     boot manager did).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpInitialize(void) {
    VidResetDisplay();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets the system display to a known state, leaving only the color unchanged.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidResetDisplay(void) {
    /* While the color/attribute is left untouched, the cursor is always reset to 0;0. */
    VidpCursorX = 0;
    VidpCursorY = 0;

    if (!(VidpAttribute & 0xF0)) {
        memset(VidpSurface, 0, 80 * 25 * 2);
    } else {
        for (int i = 0; i < 80 * 25; i++) {
            VidpSurface[i] = VidpAttribute << 8;
        }
    }
}
