/* SPDX-FileCopyrightText: (C) 2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <efi/spec.h>
#include <stdint.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function configures the video card into a mode Palalladium can use (32bpp, linear).
 *
 * PARAMETERS:
 *     Framebuffer - Output; Physical address of the frame/backbuffer.
 *     FramebufferWidth - Output; Vertical resolution of the display.
 *     FramebufferHeight - Output; Horizontal resolution of the display.
 *     FramebufferPitch - Output; How many pixels we need to skip to go into the next line.
 *
 * RETURN VALUE:
 *     EFI_SUCCESS if all went well, whichever error happened otherwise.
 *-----------------------------------------------------------------------------------------------*/
EFI_STATUS OslpInitializeGraphics(
    void **Framebuffer,
    uint32_t *FramebufferWidth,
    uint32_t *FramebufferHeight,
    uint32_t *FramebufferPitch) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *Gop = NULL;
    EFI_STATUS Status = gBS->LocateProtocol(&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **)&Gop);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to obtain the GOP (Graphics Output Protocol) handle.\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return Status;
    }

    /* QEMU seems to not have the EDID protocol? And probably, other firmwares also miss it. */
    UINT32 PreferredWidth = 1024;
    UINT32 PreferredHeight = 768;
    EFI_EDID_ACTIVE_PROTOCOL *EdidActive = NULL;
    Status = gBS->HandleProtocol(Gop, &gEfiEdidActiveProtocolGuid, (VOID **)EdidActive);
    if (Status == EFI_SUCCESS) {
        PreferredWidth = EdidActive->Edid[0x38] | ((EdidActive->Edid[0x3A] & 0xF0) << 4);
        PreferredHeight = EdidActive->Edid[0x3B] | ((EdidActive->Edid[0x3D] & 0xF0) << 4);
    }

    UINT32 BestResolution = 0;
    UINT32 BestMode = -1;
    for (UINT32 i = 0; i < Gop->Mode->MaxMode; i++) {
        UINTN SizeOfInfo = 0;
        EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info = NULL;

        Status = Gop->QueryMode(Gop, i, &SizeOfInfo, &Info);
        if (Status != EFI_SUCCESS) {
            continue;
        }

        if (Info->HorizontalResolution * Info->VerticalResolution > BestResolution &&
            Info->PixelFormat == PixelBlueGreenRedReserved8BitPerColor) {
            BestResolution = Info->HorizontalResolution * Info->VerticalResolution;
            BestMode = i;
        }

        if (Info->HorizontalResolution == PreferredWidth &&
            Info->VerticalResolution == PreferredHeight) {
            break;
        }
    }

    if (BestMode == UINT32_MAX) {
        OslPrint("Failed to find any valid display mode.\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return EFI_LOAD_ERROR;
    }

    Status = Gop->SetMode(Gop, BestMode);
    if (Status != EFI_SUCCESS) {
        OslPrint("Failed to set the display mode.\r\n");
        OslPrint("There might be something wrong with your UEFI firmware.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return Status;
    }

    *Framebuffer = (void *)Gop->Mode->FrameBufferBase;
    *FramebufferWidth = Gop->Mode->Info->HorizontalResolution;
    *FramebufferHeight = Gop->Mode->Info->VerticalResolution;
    *FramebufferPitch = Gop->Mode->Info->PixelsPerScanLine * 4;

    return EFI_SUCCESS;
}
