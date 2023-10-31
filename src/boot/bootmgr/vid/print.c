/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <font.h>
#include <string.h>

extern uint32_t *BiVideoBuffer;
extern uint16_t BiVideoWidth;
extern uint16_t BiVideoHeight;
extern uint32_t BiVideoBackground;
extern uint32_t BiVideoForeground;

uint16_t BiCursorX = 0;
uint16_t BiCursorY = 0;

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
    const BiFontGlyph *Info = &BiFont.GlyphInfo[(int)Character];
    const uint8_t *Data = &BiFont.GlyphData[Info->Offset];
    uint16_t GlyphLeft = Info->Left;
    uint16_t GlyphTop = BiFont.Ascender - Info->Top;
    uint32_t *Buffer =
        &BiVideoBuffer[(BiCursorY + GlyphTop) * BiVideoWidth + BiCursorX + GlyphLeft];

    /* We use a schema where each byte inside the glyph represents one pixel, the value of said
       pixel is the brightness of the pixel (0 being background); We just use Blend() to combine
       the background and foreground values based on the brightness. */
    for (uint16_t Top = 0; Top < Info->Height; Top++) {
        if (BiCursorY + GlyphTop + Top >= BiVideoHeight) {
            break;
        }

        for (uint16_t Left = 0; Left < Info->Width; Left++) {
            if (BiCursorX + GlyphLeft + Left >= BiVideoWidth) {
                break;
            }

            uint8_t Alpha = Data[Top * Info->Width + Left];
            if (Alpha) {
                Buffer[Top * BiVideoWidth + Left] =
                    Blend(BiVideoBackground, BiVideoForeground, Alpha);
            }
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function clears up the screen, and resets most console related variables (excluding
 *     background/foreground colors).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmResetDisplay(void) {
    for (int i = 0; i < BiVideoWidth * BiVideoHeight; i++) {
        BiVideoBuffer[i] = BiVideoBackground;
    }

    BiCursorX = 0;
    BiCursorY = 0;
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

    for (int i = 0; i < BiFont.Height; i++) {
        memset(BiVideoBuffer + (BiCursorY + i) * BiVideoWidth, 0, LeftOffset * 4);
        memset(
            BiVideoBuffer + (BiCursorY + i + 1) * BiVideoWidth - RightOffset, 0, RightOffset * 4);

        for (int j = 0; j < Size; j++) {
            BiVideoBuffer[(BiCursorY + i) * BiVideoWidth + LeftOffset + j] = BiVideoBackground;
        }
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
        BiCursorX = 0;
        BiCursorY += BiFont.Height;
    } else if (Character == '\t') {
        BiCursorX = (BiCursorX + TabSize) & ~(TabSize - 1);
    } else {
        DrawCharacter(Character);
        BiCursorX += BiFont.GlyphInfo[(int)Character].Advance;
    }

    if (BiCursorX >= BiVideoWidth) {
        BiCursorX = 0;
        BiCursorY += BiFont.Height;
    }

    if (BiCursorY >= BiVideoHeight) {
        ScrollUp();
        BiCursorY = BiVideoHeight - BiFont.Height;
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

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function calculates the exact width of a string using the default font.
 *     Use this instead of assuming the width of each character! .Advance won't always be the
 *     same value/width!
 *
 * PARAMETERS:
 *     String - What to get the size of.
 *
 * RETURN VALUE:
 *     Length in pixels of the string.
 *-----------------------------------------------------------------------------------------------*/
size_t BmGetStringWidth(const char *String) {
    size_t Size = 0;

    while (*String) {
        Size += BiFont.GlyphInfo[(int)*(String++)].Advance;
    }

    return Size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around BmPutChar for __vprint.
 *
 * PARAMETERS:
 *     Buffer - What we need to display.
 *     Size - Size of that data; The data is not required to be NULL terminated, so this need to be
 *            taken into account!
 *     Context - Always NULL for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void PutBuffer(const void *buffer, int size, void *context) {
    (void)context;
    for (int i = 0; i < size; i++) {
        BmPutChar(((const char *)buffer)[i]);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     printf() for the boot manager; Calling printf() will result in a linker error, use this
 *     instead!
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmPrint(const char *Format, ...) {
    va_list vlist;
    va_start(vlist, Format);

    __vprintf(Format, vlist, NULL, PutBuffer);

    va_end(vlist);
}
