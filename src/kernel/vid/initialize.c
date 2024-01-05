/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <vidp.h>

extern char *VidpBackBuffer;
extern char *VidpFrontBuffer;
extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint16_t VidpPitch;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves all display related data from the boot block, followed by resetting the
 *     screen. The framebuffer should be linear + 32bpp, so we don't need to implement anything
 *     special here.
 *
 * PARAMETERS:
 *     BootData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpInitialize(LoaderBootData *BootData) {
    VidpBackBuffer = (char *)BootData->Display.BackBufferBase;
    VidpFrontBuffer = (char *)BootData->Display.FrontBufferBase;
    VidpWidth = BootData->Display.Width;
    VidpHeight = BootData->Display.Height;
    VidpPitch = BootData->Display.Pitch;
    VidResetDisplay();
}
