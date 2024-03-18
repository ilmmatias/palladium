/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <cxx/lock.hxx>

extern "C" {
extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint32_t VidpBackground;
extern uint32_t VidpForeground;
extern uint16_t VidpCursorX;
extern uint16_t VidpCursorY;
extern KeSpinLock VidpLock;
}

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
extern "C" void VidSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor) {
    SpinLockGuard Guard(&VidpLock);
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
extern "C" void VidGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor) {
    SpinLockGuard Guard(&VidpLock);

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
extern "C" void VidSetCursor(uint16_t X, uint16_t Y) {
    SpinLockGuard Guard(&VidpLock);
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
extern "C" void VidGetCursor(uint16_t *X, uint16_t *Y) {
    SpinLockGuard Guard(&VidpLock);

    if (X) {
        *X = VidpCursorX;
    }

    if (Y) {
        *Y = VidpCursorY;
    }
}
