/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <font.h>
#include <string.h>

extern uint32_t *BiVideoBuffer;
extern uint16_t BiVideoWidth;
extern uint16_t BiVideoHeight;
extern uint32_t BiVideoBackground;
extern uint32_t BiVideoForeground;
extern uint16_t BiCursorX;
extern uint16_t BiCursorY;

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
void BmSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor) {
    BiVideoBackground = BackgroundColor;
    BiVideoForeground = ForegroundColor;
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
void BmGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor) {
    *BackgroundColor = BiVideoBackground;
    *ForegroundColor = BiVideoForeground;
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
void BmSetCursor(uint16_t X, uint16_t Y) {
    BiCursorX = X;
    BiCursorY = Y;
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
void BmGetCursor(uint16_t *X, uint16_t *Y) {
    *X = BiCursorX;
    *Y = BiCursorY;
}
