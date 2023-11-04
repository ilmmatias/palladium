/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>

extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint32_t VidpBackground;
extern uint32_t VidpForeground;
extern uint16_t VidpCursorX;
extern uint16_t VidpCursorY;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the background and foreground attributes of the screen.
 *
 * PARAMETERS:
 *     BackgroundColor - New background color (RGB, 24-bits).
 *     ForegroundColor - New foreground color (RGB, 24-bits).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor) {
    VidpBackground = BackgroundColor;
    VidpForeground = ForegroundColor;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current background and foreground attributes.
 *
 * PARAMETERS:
 *     BackgroundColor - Output; Current background color.
 *     ForegroundColor - Output; Current foreground color.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor) {
    if (BackgroundColor) {
        *BackgroundColor = VidpBackground;
    }

    if (ForegroundColor) {
        *ForegroundColor = VidpForeground;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the X and Y positions of the console cursor simultaneously.
 *
 * PARAMETERS:
 *     X - New X position.
 *     Y - New Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidSetCursor(uint16_t X, uint16_t Y) {
    VidpCursorX = X >= VidpWidth ? VidpWidth - 1 : X;
    VidpCursorY = Y >= VidpHeight ? VidpHeight - 1 : Y;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the X and Y positions of the console cursor simultaneously.
 *
 * PARAMETERS:
 *     X - Pointer to save location for the X position.
 *     Y - Pointer to save location for the Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidGetCursor(uint16_t *X, uint16_t *Y) {
    if (X) {
        *X = VidpCursorX;
    }

    if (Y) {
        *Y = VidpCursorY;
    }
}
