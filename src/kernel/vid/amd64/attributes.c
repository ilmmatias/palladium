/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <vid.h>

extern int VidpAttribute;
extern int VidpCursorX;
extern int VidpCursorY;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts the given color into a valid text mode attribute, and saves the
 *     value as current foreground+background.
 *
 * PARAMETERS:
 *     Color - New display color.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidSetColor(int Color) {
    if (Color == VID_COLOR_PANIC) {
        /* White on red for fatal errors. */
        VidpAttribute = 0x4F;
    } else {
        /* Gray on black otherwise. */
        VidpAttribute = 0x07;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function converts the current text mode attribute into a valid color, returning it
 *     afterwards.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Color valid for VidSetColor().
 *-----------------------------------------------------------------------------------------------*/
int VidGetColor(void) {
    return VidpAttribute == 0x4F ? VID_COLOR_PANIC : VID_COLOR_DEFAULT;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets a new display cursor position, where characters will be written starting
 *     with the next VidPutChar/String.
 *
 * PARAMETERS:
 *     X - New X position.
 *     Y - New Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidSetCursor(int X, int Y) {
    VidpCursorX = X > 79 ? 79 : X;
    VidpCursorY = Y > 24 ? 24 : Y;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the current cursor position, for later reuse.
 *
 * PARAMETERS:
 *     X - Output; Where to save the current X position.
 *     Y - Output; Where to save the current Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidGetCursor(int *X, int *Y) {
    if (X) {
        *X = VidpCursorX;
    }

    if (Y) {
        *Y = VidpCursorY;
    }
}
