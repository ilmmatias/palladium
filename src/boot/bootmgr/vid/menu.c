/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <bitmap.h>
#include <config.h>
#include <display.h>
#include <file.h>
#include <font.h>
#include <keyboard.h>
#include <loader.h>
#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <timer.h>

extern uint32_t *BiVideoBuffer;
extern uint16_t BiVideoWidth;
extern uint16_t BiVideoHeight;

static char TimeoutString[256];

static const int BmpWidth = 116;
static const int BmpHeight = 128;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function combines two pixel (background and foreground) into one, using the alpha
 *     channel.
 *
 * PARAMETERS:
 *     Background - First pixel value to be used.
 *     Foreground - Second pixel value to be used.
 *     Alpha - Value of the alpha channel.
 *
 * RETURN VALUE:
 *     Alpha blended result.
 *-----------------------------------------------------------------------------------------------*/
static uint32_t Blend(uint32_t Background, uint32_t Foreground, uint8_t Alpha) {
    uint32_t RedBlue = Background & 0xFF00FF;
    uint32_t Green = Background & 0xFF00;

    RedBlue += (((Foreground & 0xFF00FF) - RedBlue) * Alpha) >> 8;
    Green += (((Foreground & 0xFF00) - Green) * Alpha) >> 8;

    return (RedBlue & 0xFF00FF) | (Green & 0xFF00);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function draws the specified BMP file.
 *
 * PARAMETERS:
 *     Path - Full path to the BMP file.
 *     X - X coordinate.
 *     Y - Y coordinate.
 *     BackgroundColor - Color for alpha blending.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void DisplayBmp(char *Icon, uint16_t X, uint16_t Y, uint32_t BackgroundColor) {
    BitmapHeader *Header = (BitmapHeader *)Icon;

    /* No compression, no extra bit masks, 32bpp. */
    if (Header->Bpp == 32 && (Header->Compression == 0 || Header->Compression == 3)) {
        for (uint32_t i = 0; i < Header->Height; i++) {
            for (uint32_t j = 0; j < Header->Width; j++) {
                uint32_t Color =
                    *(uint32_t *)(Icon + Header->DataOffset + i * Header->Width * 4 + j * 4);
                BiVideoBuffer[(Y + (Header->Height - i)) * BiVideoWidth + X + j] =
                    Blend(BackgroundColor, Color, Color >> 24);
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function switches the selection, redrawing the previously and the currently selected
 *     icons.
 *
 * PARAMETERS:
 *     Entries - Cached menu entry data.
 *     PreviousSelection - What was the previously selected index.
 *     CurrentSelection - What's the current selected index.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void RedrawSelection(BmMenuEntry *Entries, int PreviousSelection, int CurrentSelection) {
    int Icons = BmGetMenuEntryCount();
    uint16_t X = (BiVideoWidth - Icons * BmpWidth) / 2;
    uint16_t Y = (BiVideoHeight - BmpHeight) / 2;

    /* There should be no need to clear the sections first (as the BMPs are exactly 90x128). */
    DisplayBmp(Entries[PreviousSelection].Icon, X + PreviousSelection * BmpWidth, Y, 0);
    DisplayBmp(Entries[CurrentSelection].Icon, X + CurrentSelection * BmpWidth, Y, 0x404040);

    /* We do need to clear the text though. */
    BmSetCursor(0, (BiVideoHeight - BiFont.Height) / 2 + 96);
    BmClearLine(0, 0);

    BmSetCursor(
        (BiVideoWidth - BmGetStringWidth(Entries[CurrentSelection].Text)) / 2,
        (BiVideoHeight - BiFont.Height) / 2 + 96);
    BmPutString(Entries[CurrentSelection].Text);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function redraws the timeout string.
 *
 * PARAMETERS:
 *     Timeout - How many seconds we have left.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void RedrawTimeout(int Timeout) {
    sprintf(TimeoutString, "Automatic boot in %u %s", Timeout, Timeout != 1 ? "seconds" : "second");

    BmSetCursor(0, BiVideoHeight - BiFont.Height - 16);
    BmClearLine(0, 0);

    if (Timeout >= 0) {
        BmSetCursor(
            (BiVideoWidth - BmGetStringWidth(TimeoutString)) / 2,
            BiVideoHeight - BiFont.Height - 16);
        BmPutString(TimeoutString);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function starts the GUI menu, and enters our event loop.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BiInitializeMenu(void) {
    int Icons = BmGetMenuEntryCount();
    int Timeout = BmGetDefaultTimeout();
    int Selection = BmGetDefaultSelectionIndex();
    if (Selection > Icons || Selection < 0) {
        Selection = 0;
    }

    uint16_t X = (BiVideoWidth - Icons * BmpWidth) / 2;
    uint16_t Y = (BiVideoHeight - BmpHeight) / 2;

    /* We'll be preload all menu entries (this should also read all icon files). */
    BmMenuEntry *Entries = BmAllocateZeroBlock(Icons, sizeof(BmMenuEntry));
    if (!Entries) {
        BmPrint("Could not allocate enough memory for all menu entries.\n"
                "Your system might not have enough usable memory.\n");
        while (1)
            ;
    }

    for (int i = 0; i < Icons; i++) {
        BmGetMenuEntry(i, &Entries[i]);
    }

    /* Initial display; Later on, we'll be only redrawing what's necessary. */
    for (int i = 0; i < Icons; i++) {
        DisplayBmp(Entries[i].Icon, X + i * BmpWidth, Y, i == Selection ? 0x404040 : 0);
    }

    const char *String = "Palladium Boot Manager";
    BmSetCursor((BiVideoWidth - BmGetStringWidth(String)) / 2, 16);
    BmPutString(String);

    BmSetCursor(
        (BiVideoWidth - BmGetStringWidth(Entries[Selection].Text)) / 2,
        (BiVideoHeight - BiFont.Height) / 2 + 96);
    BmPutString(Entries[Selection].Text);

    /* 0 means there's no countdown, kind of. We still wait 1 second, but we change the text
       message. */
    if (Timeout) {
        sprintf(TimeoutString, "Automatic boot in %u %s", Timeout, Timeout != 1 ? "seconds" : "second");
        BmSetCursor(
            (BiVideoWidth - BmGetStringWidth(TimeoutString)) / 2,
            BiVideoHeight - BiFont.Height - 16);
        BmPutString(TimeoutString);
    } else {
        String = "Automatic boot enabled, press any key to stop";
        BmSetCursor(
            (BiVideoWidth - BmGetStringWidth(String)) / 2,
            BiVideoHeight - BiFont.Height - 16);
        BmPutString(String);
    }

    BmSetupTimer();
    while (1) {
        int Key;
        uint64_t ElapsedTime;

        while ((Key = BmPollKey()) == -1 && (ElapsedTime = BmGetElapsedTime()) < 1)
            ;

        if (Key != -1 && Key != '\n') {
            if (Key == KEY_LEFT) {
                if (Selection > 0) {
                    RedrawSelection(Entries, Selection, Selection - 1);
                    Selection--;
                }
            } else if (Key == KEY_RIGHT) {
                if (Selection + 1 < Icons) {
                    RedrawSelection(Entries, Selection, Selection + 1);
                    Selection++;
                }
            } 

            Timeout = -1;
            RedrawTimeout(-1);
        } else if (Key == '\n' || !Timeout || Timeout == 1) {
            BiLoadEntry(&Entries[Selection]);
        } else if (ElapsedTime && Timeout > 0) {
            BmSetupTimer();
            RedrawTimeout(--Timeout);
        }
    }
}
