/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/mi.h>
#include <kernel/vidp.h>

extern char *VidpBackBuffer;
extern char *VidpFrontBuffer;
extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint16_t VidpPitch;
extern bool VidpUseLock;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves all display related data from the boot block, and resets the display (to
 *     remove any data still visible from osloader).
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpInitialize(KiLoaderBlock *LoaderBlock) {
    VidpBackBuffer = LoaderBlock->BackBuffer;
    VidpFrontBuffer = LoaderBlock->FrontBuffer;
    VidpWidth = LoaderBlock->FramebufferWidth;
    VidpHeight = LoaderBlock->FramebufferHeight;
    VidpPitch = LoaderBlock->FramebufferPitch;
    VidResetDisplay();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function disables the KeAcquireSpinLocks inside the Vid* functions.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpAcquireOwnership(void) {
    VidpUseLock = false;
}
