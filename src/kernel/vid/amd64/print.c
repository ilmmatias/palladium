/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <string.h>
#include <vid.h>

extern uint16_t *VidpSurface;
extern int VidpAttribute;
extern int VidpCursorX;
extern int VidpCursorY;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function displaces all lines one slot up, giving way for a new line at the bottom.
 *
 * PARAMETERS:
 *     Color - New attributes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ScrollUp(void) {
    memmove(VidpSurface, VidpSurface + 80, 80 * 24 * 2);
    memset(VidpSurface + 80 * 24, 0, 80 * 2);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function displays a single character to the screen, scrolling up if required.
 *
 * PARAMETERS:
 *     Character - Data to be displayed.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPutChar(char Character) {
    if (Character == '\n') {
        VidpCursorX = 0;
        VidpCursorY++;
    } else if (Character == '\t') {
        VidpCursorX = (VidpCursorX + 4) & ~3;
    } else {
        VidpSurface[VidpCursorY * 80 + VidpCursorX] = VidpAttribute << 8 | Character;
        VidpCursorX++;
    }

    if (VidpCursorX >= 80) {
        VidpCursorX = 0;
        VidpCursorY++;
    }

    if (VidpCursorY >= 25) {
        ScrollUp();
        VidpCursorY = 24;
    }
}
