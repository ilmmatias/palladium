/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <string.h>
#include <x86/bios.h>

char *BiVideoBuffer = NULL;
uint16_t BiVideoWidth = 0;
uint16_t BiVideoHeight = 0;
uint16_t BiVideoPitch = 0;
uint32_t BiVideoBackground = 0x000000;
uint32_t BiVideoForeground = 0xAAAAAA;

typedef struct __attribute__((packed)) {
    char VbeSignature[4];
    uint16_t VbeVersion;
    uint16_t OemStringPtr[2];
    uint8_t Capabilities[4];
    uint16_t VideoModeOff;
    uint16_t VideoModeSeg;
    uint16_t TotalMemory;
    uint8_t Reserved[492];
} VbeInfoBlock;

typedef struct __attribute__((packed)) {
    uint16_t Attributes;
    uint8_t WindowA;
    uint8_t WindowB;
    uint16_t Granularity;
    uint16_t WindowSize;
    uint16_t SegmentA;
    uint16_t SegmentB;
    uint32_t WinFuncPtr;
    uint16_t Pitch;
    uint16_t Width;
    uint16_t Height;
    uint8_t WChar;
    uint8_t YChar;
    uint8_t Planes;
    uint8_t Bpp;
    uint8_t Banks;
    uint8_t MemoryModel;
    uint8_t BankSize;
    uint8_t ImagePages;
    uint8_t Reserved0;
    uint8_t RedMask;
    uint8_t RedPosition;
    uint8_t GreenMask;
    uint8_t GreenPosition;
    uint8_t BlueMask;
    uint8_t BluePosition;
    uint8_t ReservedMask;
    uint8_t ReservedPosition;
    uint8_t DirectColorAttributes;
    uint32_t Framebuffer;
    uint32_t OffScreenMemOff;
    uint16_t OffScreenMemSize;
    uint8_t Reserved1[206];
} VbeModeInfo;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function draws text when we fail to initialize the display, halting the system
 *     afterwards.
 *
 * PARAMETERS:
 *     Text - What we should draw.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] static void DrawTextModeError(const char *Text) {
    /* 80x25x2 */
    memset((void *)0xB8000, 0, 4000);

    for (size_t i = 0; Text[i]; i++) {
        *(uint16_t *)(0xB8000 + i * 2) = Text[i] | 0x4F00;
    }

    while (1)
        ;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the video mode on first call, followed by resetting the screen/console
 *     data. Any further reinitialization should be done through BmResetDisplay.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BiInitializeDisplay(void) {
    BiosRegisters Registers;

    VbeInfoBlock InfoBlock;
    VbeModeInfo ModeInfo;

    int PreferredWidth = 800, PreferredHeight = 600;
    char Edid[128];

    /* The info block contains the list of all valid modes, so grab it first (assume we're in an
       incompatible system if it doesn't exist). */
    memset(&InfoBlock, 0, sizeof(VbeInfoBlock));
    memcpy(InfoBlock.VbeSignature, "VBE2", 4);

    memset(&Registers, 0, sizeof(BiosRegisters));
    Registers.Eax = 0x4F00;
    Registers.Edi = (uint32_t)&InfoBlock;

    BiosCall(0x10, &Registers);
    if (Registers.Eax != 0x4F) {
        DrawTextModeError("Could not setup graphical mode on this device.");
    }

    /* Grab the EDID, we want the preferred resolution if possible (though we might grab something
       smaller if it isn't 32bpp). */
    memset(&Registers, 0, sizeof(BiosRegisters));
    Registers.Eax = 0x4F15;
    Registers.Ebx = 1;
    Registers.Edi = (uint32_t)&Edid;

    BiosCall(0x10, &Registers);
    if (Registers.Eax == 0x4F) {
        // PreferredWidth = Edid[0x38] | ((Edid[0x3A] & 0xF0) << 4);
        // PreferredHeight = Edid[0x3B] | ((Edid[0x3D] & 0xF0) << 4);
    }

    uint16_t *Modes = (uint16_t *)((InfoBlock.VideoModeSeg << 4) + InfoBlock.VideoModeOff);
    int BestResolution = 0;
    int BestMode;
    uint16_t BestWidth;
    uint16_t BestHeight;
    uint16_t BestPitch;
    uint32_t BestFramebuffer;

    for (int i = 0; Modes[i] != UINT16_MAX; i++) {
        memset(&Registers, 0, sizeof(BiosRegisters));
        Registers.Eax = 0x4F01;
        Registers.Ecx = Modes[i];
        Registers.Edi = (uint32_t)&ModeInfo;

        BiosCall(0x10, &Registers);
        if (Registers.Eax != 0x4F) {
            continue;
        }

        if (ModeInfo.MemoryModel != 0x06 || !(ModeInfo.Attributes & 0x80) || ModeInfo.Bpp != 32 ||
            ModeInfo.Width * ModeInfo.Height < BestResolution) {
            continue;
        }

        BestMode = Modes[i];
        BestResolution = ModeInfo.Width * ModeInfo.Height;
        BestWidth = ModeInfo.Width;
        BestHeight = ModeInfo.Height;
        BestPitch = ModeInfo.Pitch;
        BestFramebuffer = ModeInfo.Framebuffer;

        /* No need to search further on an exact match. */
        if (ModeInfo.Width == PreferredWidth && ModeInfo.Height == PreferredHeight) {
            break;
        }
    }

    if (!BestResolution) {
        DrawTextModeError("Could not setup graphical mode on this device.");
    }

    /* Wrap up by setting the graphics mode, and continuing into clearing the screen. */
    memset(&Registers, 0, sizeof(BiosRegisters));
    Registers.Eax = 0x4F02;
    Registers.Ebx = BestMode | 0x4000;
    BiosCall(0x10, &Registers);

    BiVideoBuffer = (char *)BestFramebuffer;
    BiVideoWidth = BestWidth;
    BiVideoHeight = BestHeight;
    BiVideoPitch = BestPitch;

    BmResetDisplay();
}
