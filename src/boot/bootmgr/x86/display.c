/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <stdint.h>
#include <string.h>
#include <x86/bios.h>

uint32_t *BiVideoBuffer = NULL;
uint16_t BiVideoWidth = 0;
uint16_t BiVideoHeight = 0;
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
 *     This function sets the video mode on first call, followed by resetting the screen/console
 *     data. Any further reinitialization should be done through BmResetDisplay.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmInitDisplay(void) {
    VbeInfoBlock InfoBlock;
    VbeModeInfo ModeInfo;
    BiosRegisters Registers;

    /* The info block contains the list of all valid modes, so grab it first (assume we're in an
    incompatible system if it doesn't exist). */
    memset(&InfoBlock, 0, sizeof(VbeInfoBlock));
    memcpy(InfoBlock.VbeSignature, "VBE2", 4);

    memset(&Registers, 0, sizeof(BiosRegisters));
    Registers.Eax = 0x4F00;
    Registers.Edi = (uint32_t)&InfoBlock;

    BiosCall(0x10, &Registers);
    if (Registers.Eax != 0x4F) {
        /* TODO: Print something using the legacy 0xB8000 text buffer. */
        while (1)
            ;
    }

    /* We're aiming for at most 1024x768, but anything that is lower than that (and 32bpp) is
       good too. */
    static const int MaxWidth = 1024, MaxHeight = 768;

    uint16_t *Modes = (uint16_t *)((InfoBlock.VideoModeSeg << 4) + InfoBlock.VideoModeOff);
    int BestResolution = 0;
    int BestMode;
    uint16_t BestWidth;
    uint16_t BestHeight;
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

        if (ModeInfo.MemoryModel != 0x06 || !(ModeInfo.Attributes & 0x80) || ModeInfo.Bpp != 32) {
            continue;
        } else if (
            ModeInfo.Width * ModeInfo.Height < BestResolution || ModeInfo.Width > MaxWidth ||
            ModeInfo.Height > MaxHeight) {
            continue;
        }

        BestMode = Modes[i];
        BestResolution = ModeInfo.Width * ModeInfo.Height;
        BestWidth = ModeInfo.Width;
        BestHeight = ModeInfo.Height;
        BestFramebuffer = ModeInfo.Framebuffer;

        /* No need to search further on an exact match. */
        if (ModeInfo.Width == MaxWidth && ModeInfo.Height == MaxHeight) {
            break;
        }
    }

    if (!BestResolution) {
        while (1)
            ;
    }

    /* Wrap up by setting the graphics mode, and continuing into clearing the screen. */
    memset(&Registers, 0, sizeof(BiosRegisters));
    Registers.Eax = 0x4F02;
    Registers.Ebx = BestMode | 0x4000;
    BiosCall(0x10, &Registers);

    BiVideoBuffer = (uint32_t *)BestFramebuffer;
    BiVideoWidth = BestWidth;
    BiVideoHeight = BestHeight;

    BmResetDisplay();
}
