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
extern int VidpUseLock;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves all display related data from the boot block, and maps the back buffer
 *     into virtual memory, allowing us to use KeFatalError.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpSaveDisplayData(KiLoaderBlock *LoaderBlock) {
    VidpWidth = LoaderBlock->FramebufferWidth;
    VidpHeight = LoaderBlock->FramebufferHeight;
    VidpPitch = LoaderBlock->FramebufferPitch;

    /* As the display still isn't initialized, this is the only init stage that can't KeFatalError,
     * so we just gotta halt execution (and not show any error messages). */
    VidpBackBuffer = MmMapSpace((uint64_t)LoaderBlock->Framebuffer, VidpHeight * VidpPitch * 4);
    if (!VidpBackBuffer) {
        while (1) {
            HalpStopProcessor();
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function finishes up the display initialization by allocating the front buffer.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpCreateFrontBuffer(void) {
    VidpFrontBuffer = MmAllocatePool(VidpHeight * VidpPitch * 4, "Vidp");
    if (!VidpFrontBuffer) {
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    VidResetDisplay();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function disables the KeAcquireSpinLock()s inside the Vid* functions.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidpAcquireOwnership(void) {
    VidpUseLock = 0;
}
