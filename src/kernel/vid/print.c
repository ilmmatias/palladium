/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stddef.h>
#include <string.h>
#include <vidp.h>

extern const VidpFontData VidpFont;

uint32_t *VidpBuffer = NULL;
uint16_t VidpWidth = 0;
uint16_t VidpHeight = 0;

uint32_t VidpBackground = 0x000000;
uint32_t VidpForeground = 0xAAAAAA;
uint16_t VidpCursorX = 0;
uint16_t VidpCursorY = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function displaces all lines one slot up, giving way for a new line at the bottom.
 *
 * PARAMETERS:
 *     Color - New attributes.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ScrollUp(void) {
    const uint32_t ScreenSize = VidpWidth * VidpHeight;
    const uint32_t LineSize = VidpWidth * VidpFont.Height;
    memmove(VidpBuffer, VidpBuffer + LineSize, (ScreenSize - LineSize) * 4);
    memset(VidpBuffer + ScreenSize - LineSize, 0, LineSize * 4);
}

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
 *     This function draws a character into the display, using the system/boot font.
 *
 * PARAMETERS:
 *     Character - What to be displayed.
 *
 * RETURN VALUE:
 *     None
 *-----------------------------------------------------------------------------------------------*/
static void DrawCharacter(char Character) {
    const VidpFontGlyph *Info = &VidpFont.GlyphInfo[(int)Character];
    const uint8_t *Data = &VidpFont.GlyphData[Info->Offset];
    uint16_t GlyphLeft = Info->Left;
    uint16_t GlyphTop = VidpFont.Ascender - Info->Top;
    uint32_t *Buffer = &VidpBuffer[(VidpCursorY + GlyphTop) * VidpWidth + VidpCursorX + GlyphLeft];

    /* We use a schema where each byte inside the glyph represents one pixel, the value of said
       pixel is the brightness of the pixel (0 being background); We just use Blend() to combine
       the background and foreground values based on the brightness. */
    for (uint16_t Top = 0; Top < Info->Height; Top++) {
        if (VidpCursorY + GlyphTop + Top >= VidpHeight) {
            break;
        }

        for (uint16_t Left = 0; Left < Info->Width; Left++) {
            if (VidpCursorX + GlyphLeft + Left >= VidpWidth) {
                break;
            }

            uint8_t Alpha = Data[Top * Info->Width + Left];
            if (Alpha) {
                Buffer[Top * VidpWidth + Left] = Blend(VidpBackground, VidpForeground, Alpha);
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function resets the system display to a known state, leaving only the color unchanged.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidResetDisplay(void) {
    /* While the color/attribute is left untouched, the cursor is always reset to 0;0. */
    VidpCursorX = 0;
    VidpCursorY = 0;

    for (int i = 0; i < VidpWidth * VidpHeight; i++) {
        VidpBuffer[i] = VidpBackground;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function displays a character using the current bg/fg attribute values.
 *
 * PARAMETERS:
 *     Character - The character.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPutChar(char Character) {
    /* It's probably safe to use the size of 4 spaces? */
    const int TabSize = VidpFont.GlyphInfo[0x20].Width * 4;

    if (Character == '\n') {
        VidpCursorX = 0;
        VidpCursorY += VidpFont.Height;
    } else if (Character == '\t') {
        VidpCursorX = (VidpCursorX + TabSize) & ~(TabSize - 1);
    } else {
        DrawCharacter(Character);
        VidpCursorX += VidpFont.GlyphInfo[(int)Character].Advance;
    }

    if (VidpCursorX >= VidpWidth) {
        VidpCursorX = 0;
        VidpCursorY += VidpFont.Height;
    }

    if (VidpCursorY >= VidpHeight) {
        ScrollUp();
        VidpCursorY = VidpHeight - VidpFont.Height;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs the specified character buffer into the screen.
 *
 * PARAMETERS:
 *     String - What to output.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPutString(const char *String) {
    if (String) {
        while (*String) {
            VidPutChar(*(String++));
        }
    }
}
