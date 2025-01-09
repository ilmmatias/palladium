/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mi.h>
#include <vidp.h>

extern char *VidpBackBuffer;
extern char *VidpFrontBuffer;
extern uint16_t VidpWidth;
extern uint16_t VidpHeight;
extern uint16_t VidpPitch;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves all display related data from the boot block, and follows up by
 *     allocating the front buffer and mapping the back buffer into virtual memory.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpInitialize(KiLoaderBlock *LoaderBlock) {
    VidpWidth = LoaderBlock->FramebufferWidth;
    VidpHeight = LoaderBlock->FramebufferHeight;
    VidpPitch = LoaderBlock->FramebufferPitch;

    /* We can't really fail until the display is initialized (calling KeFatalError would triple
     * fault), so let's hope this works, or we'll be triple faulting anyways. */
    VidpBackBuffer = MmMapSpace((uint64_t)LoaderBlock->Framebuffer, VidpHeight * VidpPitch * 4);
    VidpFrontBuffer = MmAllocatePool(VidpHeight * VidpPitch * 4, "Vidp");
    for (uint64_t i = 0; i < VidpHeight * VidpPitch * 4; i += MM_PAGE_SIZE) {
        HalpMapPage(
            (char *)VidpBackBuffer + i,
            (uint64_t)LoaderBlock->Framebuffer + i,
            MI_MAP_WRITE | MI_MAP_DEVICE);
    }

    VidResetDisplay();
}
