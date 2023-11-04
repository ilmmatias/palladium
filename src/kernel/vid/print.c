/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <ke.h>
#include <string.h>
#include <vidp.h>

extern const VidpFontData VidpFont;

uint32_t *VidpBackBuffer = NULL;
uint32_t *VidpFrontBuffer = NULL;
uint16_t VidpWidth = 0;
uint16_t VidpHeight = 0;

uint32_t VidpBackground = 0x000000;
uint32_t VidpForeground = 0xAAAAAA;
uint16_t VidpCursorX = 0;
uint16_t VidpCursorY = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function copies the front buffer into the back buffer (flusing its contents into the
 *     screen).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void Flush(void) {
    memcpy(VidpBackBuffer, VidpFrontBuffer, VidpWidth * VidpHeight * 4);
}

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
    memmove(VidpFrontBuffer, VidpFrontBuffer + LineSize, (ScreenSize - LineSize) * 4);
    memset(VidpFrontBuffer + ScreenSize - LineSize, 0, LineSize * 4);
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
    uint32_t *Buffer =
        &VidpFrontBuffer[(VidpCursorY + GlyphTop) * VidpWidth + VidpCursorX + GlyphLeft];

    /* The code after us only cares about the foreground, so we're left to manually clear the
       background. */
    for (uint16_t Top = 0; Top < VidpFont.Height; Top++) {
        if (VidpCursorY + Top >= VidpHeight) {
            break;
        }

        for (uint16_t Left = 0; Left < VidpFont.Ascender + VidpFont.Descender; Left++) {
            if (VidpCursorX + Left >= VidpWidth) {
                break;
            }

            VidpFrontBuffer[(VidpCursorY + Top) * VidpWidth + VidpCursorX + Left] = VidpBackground;
        }
    }

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
        VidpFrontBuffer[i] = VidpBackground;
    }

    Flush();
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
    int NeedsFlush = 0;

    if (Character == '\n') {
        VidpCursorX = 0;
        VidpCursorY += VidpFont.Height;
        NeedsFlush = 1;
    } else if (Character == '\t') {
        VidpCursorX = (VidpCursorX + TabSize) & ~(TabSize - 1);
    } else {
        DrawCharacter(Character);
        VidpCursorX += VidpFont.GlyphInfo[(int)Character].Advance;
    }

    if (VidpCursorX >= VidpWidth) {
        VidpCursorX = 0;
        VidpCursorY += VidpFont.Height;
        NeedsFlush = 1;
    }

    if (VidpCursorY >= VidpHeight) {
        ScrollUp();
        VidpCursorY = VidpHeight - VidpFont.Height;
    }

    if (NeedsFlush) {
        Flush();
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
    while (*String) {
        VidPutChar(*(String++));
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around VidPutChar for __vprint.
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
        VidPutChar(((const char *)buffer)[i]);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the screen; For most cases, you probably want VidPrint,
 *     but you can use this to "rename" the VidPrint function, if your driver needs/wants that.
 *
 * PARAMETERS:
 *     Type - Type of the message; This is used to set up the prefix and its color.
 *     Prefix - Name of the driver/module (to be attached to the start of the message).
 *     Message - Format string; Works the same as printf().
 *     Arguments - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPrintVariadic(int Type, const char *Prefix, const char *Message, va_list Arguments) {
    /* Make sure this specific type of message wasn't disabled at compile time. */
    switch (Type) {
        case KE_MESSAGE_TRACE:
            if (!KE_ENABLE_MESSAGE_TRACE) {
                return;
            }

            break;
        case KE_MESSAGE_DEBUG:
            if (!KE_ENABLE_MESSAGE_DEBUG) {
                return;
            }

            break;
        case KE_MESSAGE_INFO:
            if (!KE_ENABLE_MESSAGE_INFO) {
                return;
            }

            break;
    }

    uint32_t OriginalBackground;
    uint32_t OriginalForeground;
    VidGetColor(&OriginalBackground, &OriginalForeground);

    const char *Suffix;
    switch (Type) {
        case KE_MESSAGE_ERROR:
            VidSetColor(OriginalBackground, 0xFF0000);
            Suffix = " Error: ";
            break;
        case KE_MESSAGE_TRACE:
            VidSetColor(OriginalBackground, 0x00FF00);
            Suffix = " Trace: ";
            break;
        case KE_MESSAGE_DEBUG:
            VidSetColor(OriginalBackground, 0xFFFF00);
            Suffix = " Debug: ";
            break;
        default:
            VidSetColor(OriginalBackground, 0x0000FF);
            Suffix = " Info: ";
            break;
    }

    VidPutString(Prefix);
    VidPutString(Suffix);
    VidSetColor(OriginalBackground, OriginalForeground);

    __vprintf(Message, Arguments, NULL, PutBuffer);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the screen.
 *
 * PARAMETERS:
 *     Type - Type of the message; Depending on how the kernel was built, the message won't show.
 *     Prefix - Name of the driver/module (to be attached to the start of the message).
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPrint(int Type, const char *Prefix, const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Format);
    VidPrintVariadic(Type, Prefix, Message, Arguments);
    va_end(Arguments);
}
