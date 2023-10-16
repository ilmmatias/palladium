/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>
#include <vid.h>

extern uint32_t *VidpBuffer;
extern uint16_t VidpWidth;
extern uint16_t VidpHeight;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves all display related data from the boot block, followed by resetting the
 *     screen. The framebuffer should be linear + 32bpp, so we don't need to implement anything
 *     special here.
 *
 * PARAMETERS:
 *     LoaderData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpInitialize(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;
    VidpBuffer = (uint32_t *)BootData->Display.BaseAddress;
    VidpWidth = BootData->Display.Width;
    VidpHeight = BootData->Display.Height;
    VidResetDisplay();
}
