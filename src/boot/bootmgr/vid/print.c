/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <font.h>
#include <string.h>

extern uint32_t *BiVideoBuffer;
extern uint16_t BiVideoWidth;
extern uint16_t BiVideoHeight;
extern uint32_t BiVideoBackground;
extern uint32_t BiVideoForeground;

static uint16_t CursorX = 0;
static uint16_t CursorY = 0;

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
    const uint32_t ScreenSize = BiVideoWidth * BiVideoHeight;
    const uint32_t LineSize = BiVideoWidth * BiFont.Height;
    memmove(BiVideoBuffer, BiVideoBuffer + LineSize, (ScreenSize - LineSize) * 4);
    memset(BiVideoBuffer + ScreenSize - LineSize, 0, LineSize * 4);
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
static uint8_t Blend(uint32_t Background, uint32_t Foreground, uint8_t Alpha) {
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
    const BiFontGlyph *Info = &BiFont.GlyphInfo[(int)Character];
    const uint8_t *Data = &BiFont.GlyphData[Info->Offset];
    uint16_t GlyphLeft = Info->Left;
    uint16_t GlyphTop = BiFont.Ascender - Info->Top;
    uint32_t *Buffer = &BiVideoBuffer[(CursorY + GlyphTop) * BiVideoWidth + CursorX + GlyphLeft];

    /* We use a schema where each byte inside the glyph represents one pixel, the value of said
       pixel is the brightness of the pixel (0 being background); We just use Blend() to combine
       the background and foreground values based on the brightness. */
    for (uint16_t Top = 0; Top < Info->Height; Top++) {
        if (CursorY + GlyphTop + Top >= BiVideoHeight) {
            break;
        }

        for (uint16_t Left = 0; Left < Info->Width; Left++) {
            if (CursorX + GlyphLeft + Left >= BiVideoWidth) {
                break;
            }

            uint8_t Alpha = Data[Top * Info->Width + Left];
            uint32_t *BufferPos = &Buffer[Top * BiVideoWidth + Left];

            if (Alpha) {
                *BufferPos = Blend(*BufferPos, BiVideoForeground, Alpha);
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the background and foreground attributes of the screen.
 *
 * PARAMETERS:
 *     BackgroundColor - New background color (RGB, full 32-bits).
 *     ForegroundColor - New foreground color (RGB, full 32-bits).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmSetColor(uint32_t BackgroundColor, uint32_t ForegroundColor) {
    BiVideoBackground = BackgroundColor;
    BiVideoForeground = ForegroundColor;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current background and foreground attributes.
 *
 * PARAMETERS:
 *     BackgroundColor - Output; Current background color.
 *     ForegroundColor - Output; Current foreground color.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmGetColor(uint32_t *BackgroundColor, uint32_t *ForegroundColor) {
    *BackgroundColor = BiVideoBackground;
    *ForegroundColor = BiVideoForeground;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the X and Y positions of the console cursor simultaneously.
 *
 * PARAMETERS:
 *     X - New X position.
 *     Y - New Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmSetCursor(uint16_t X, uint16_t Y) {
    CursorX = X;
    CursorY = Y;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the X and Y positions of the console cursor simultaneously.
 *
 * PARAMETERS:
 *     X - Pointer to save location for the X position.
 *     Y - Pointer to save location for the Y position.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmGetCursor(uint16_t *X, uint16_t *Y) {
    *X = CursorX;
    *Y = CursorY;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function clears the current line, using the current attribute background.
 *
 * PARAMETERS:
 *     LeftOffset - How many spaces should we put (with no background) on the left side.
 *     RightOffset - How many spaces should we put (with no background) on the right side.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmClearLine(int LeftOffset, int RightOffset) {
    int Size = BiVideoWidth - LeftOffset - RightOffset;

    memset(BiVideoBuffer + CursorY * BiVideoWidth, 0, LeftOffset * 4);
    memset(BiVideoBuffer + (CursorY + 1) * BiVideoWidth - RightOffset, 0, RightOffset * 4);

    for (int i = 0; i < Size; i++) {
        BiVideoBuffer[CursorY * BiVideoWidth + LeftOffset + i] = BiVideoBackground;
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
void BmPutChar(char Character) {
    /* It's probably safe to use the size of 4 spaces? */
    const int TabSize = BiFont.GlyphInfo[0x20].Width * 4;

    if (Character == '\n') {
        CursorX = 0;
        CursorY += BiFont.Height;
    } else if (Character == '\t') {
        CursorX = (CursorX + TabSize) & ~(TabSize - 1);
    } else {
        DrawCharacter(Character);
        CursorX += BiFont.GlyphInfo[(int)Character].Advance;
    }

    if (CursorX >= BiVideoWidth) {
        CursorX = 0;
        CursorY += BiFont.Height;
    }

    if (CursorY >= BiVideoHeight) {
        ScrollUp();
        CursorY = BiFont.Height - BiFont.Height;
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
void BmPutString(const char *String) {
    while (*String) {
        BmPutChar(*(String++));
    }
}
