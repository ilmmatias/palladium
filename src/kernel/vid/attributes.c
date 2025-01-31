/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ke.h>

extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint32_t VidpBackground;
extern uint32_t VidpForeground;
extern uint16_t VidpCursorX;
extern uint16_t VidpCursorY;
extern KeSpinLock VidpLock;
extern bool VidpUseLock;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the display lock if required.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     IRQL used for releasing the lock.
 *-----------------------------------------------------------------------------------------------*/
static KeIrql AcquireSpinLock(void) {
    if (VidpUseLock) {
        return KeAcquireSpinLockAndRaiseIrql(&VidpLock, KE_IRQL_DISPATCH);
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the display lock if required.
 *
 * PARAMETERS:
 *     OldIrql - Return value of AcquireSpinLock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ReleaseSpinLock(KeIrql OldIrql) {
    if (VidpUseLock) {
        KeReleaseSpinLockAndLowerIrql(&VidpLock, OldIrql);
    }
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
void VidSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor) {
    KeIrql OldIrql = AcquireSpinLock();
    VidpBackground = BackgroundColor;
    VidpForeground = ForegroundColor;
    ReleaseSpinLock(OldIrql);
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
    KeIrql OldIrql = AcquireSpinLock();

    if (BackgroundColor) {
        *BackgroundColor = VidpBackground;
    }

    if (ForegroundColor) {
        *ForegroundColor = VidpForeground;
    }

    ReleaseSpinLock(OldIrql);
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
    KeIrql OldIrql = AcquireSpinLock();
    VidpCursorX = X >= VidpWidth ? VidpWidth - 1 : X;
    VidpCursorY = Y >= VidpHeight ? VidpHeight - 1 : Y;
    ReleaseSpinLock(OldIrql);
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
    KeIrql OldIrql = AcquireSpinLock();

    if (X) {
        *X = VidpCursorX;
    }

    if (Y) {
        *Y = VidpCursorY;
    }

    ReleaseSpinLock(OldIrql);
}
