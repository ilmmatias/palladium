/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/fmt.h>
#include <kernel/ke.h>
#include <kernel/vidp.h>
#include <string.h>

extern const VidpFontData VidpFont;

static bool PendingFullFlush = false;
static uint16_t FlushY = 0;
static uint16_t FlushLines = 0;

char *VidpBackBuffer = NULL;
char *VidpFrontBuffer = NULL;
uint16_t VidpWidth = 0;
uint16_t VidpHeight = 0;
uint16_t VidpPitch = 0;

uint32_t VidpBackground = 0x000000;
uint32_t VidpForeground = 0xAAAAAA;
uint16_t VidpCursorX = 0;
uint16_t VidpCursorY = 0;

KeSpinLock VidpLock = {0};
bool VidpUseLock = true;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the display lock if required.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     IRQL used for releasing the lock.
 *-----------------------------------------------------------------------------------------------*/
static KeIrql AcquireSpinLock(void) {
    if (VidpUseLock) {
        return KeAcquireSpinLockAndRaiseIrql(&VidpLock, KE_IRQL_DISPATCH);
    } else {
        return 0;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the display lock if required.
 *
 * PARAMETERS:
 *     OldIrql - Return value of AcquireSpinLock.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void ReleaseSpinLock(KeIrql OldIrql) {
    if (VidpUseLock) {
        KeReleaseSpinLockAndLowerIrql(&VidpLock, OldIrql);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function copies the front buffer into the back buffer (flushing its contents into the
 *     screen).
 *     Set the contents of FlushY/Lines according to what region you want to flush.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void Flush(void) {
    memcpy(
        VidpBackBuffer + FlushY * VidpPitch,
        VidpFrontBuffer + FlushY * VidpPitch,
        FlushLines * VidpPitch);
    PendingFullFlush = false;
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
    const uint32_t ScreenSize = VidpPitch * VidpHeight;
    const uint32_t LineSize = VidpPitch * VidpFont.Height;
    memmove(VidpFrontBuffer, VidpFrontBuffer + LineSize, ScreenSize - LineSize);

    /* We can't just memset, thanks to the background not being fixed at 0x00000000 (black). */
    for (int i = 0; i < VidpFont.Height; i++) {
        for (int j = 0; j < VidpWidth; j++) {
            *(uint32_t *)&VidpFrontBuffer[ScreenSize - LineSize + i * VidpPitch + j * 4] =
                VidpBackground;
        }
    }

    PendingFullFlush = true;
    FlushY = 0;
    FlushLines = VidpHeight;
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
    char *Buffer =
        &VidpFrontBuffer[(VidpCursorY + GlyphTop) * VidpPitch + (VidpCursorX + GlyphLeft) * 4];

    /* The code after us only cares about the foreground, so we're left to manually clear the
       background. */
    for (uint16_t Top = 0; Top < VidpFont.Height; Top++) {
        if (VidpCursorY + Top >= VidpHeight) {
            break;
        }

        for (uint16_t Left = 0; Left < VidpFont.GlyphInfo[0x20].Advance; Left++) {
            if (VidpCursorX + Left >= VidpWidth) {
                break;
            }

            *(uint32_t
                  *)&VidpFrontBuffer[(VidpCursorY + Top) * VidpPitch + (VidpCursorX + Left) * 4] =
                VidpBackground;
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
                *(uint32_t *)&Buffer[Top * VidpPitch + Left * 4] =
                    Blend(VidpBackground, VidpForeground, Alpha);
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
    KeIrql OldIrql = AcquireSpinLock();
    VidpCursorX = 0;
    VidpCursorY = 0;

    for (int i = 0; i < VidpHeight; i++) {
        for (int j = 0; j < VidpWidth; j++) {
            *(uint32_t *)&VidpFrontBuffer[i * VidpPitch + j * 4] = VidpBackground;
        }
    }

    FlushY = 0;
    FlushLines = VidpHeight;
    Flush();
    ReleaseSpinLock(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Internal "display character and update attributes" function; Only call this after acquiring
 *     the lock!
 *
 * PARAMETERS:
 *     Character - The character.
 *
 * RETURN VALUE:
 *     None.
 *------------------------------------------------------------------------------------------------*/
static void PutChar(char Character) {
    /* It's probably safe to use the size of 4 spaces? */
    const int TabSize = VidpFont.GlyphInfo[0x20].Width * 4;

    if (VidpCursorY + VidpFont.Height > VidpHeight) {
        ScrollUp();
        VidpCursorY -= VidpFont.Height;
    }

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
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Unlocked version of VidPutString; Only call this after acquiring the lock!
 *
 * PARAMETERS:
 *     String - What to output.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void PutString(const char *String) {
    while (*String) {
        PutChar(*(String++));
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
        PutChar(((const char *)buffer)[i]);
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
    KeIrql OldIrql = AcquireSpinLock();

    FlushY = VidpCursorY;
    PutChar(Character);

    if (!PendingFullFlush) {
        FlushLines = VidpCursorY - FlushY + VidpFont.Height;
    }

    Flush();
    ReleaseSpinLock(OldIrql);
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
    KeIrql OldIrql = AcquireSpinLock();

    FlushY = VidpCursorY;
    PutString(String);

    if (!PendingFullFlush) {
        FlushLines = VidpCursorY - FlushY + VidpFont.Height;
    }

    Flush();
    ReleaseSpinLock(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a non-prefixed message into the screen (we're the kernel equivalent
 *     of vprintf); For most cases, you probably want VidPrintSimple instead of this, but you can
 *     use this to "rename" the VidPrintSimple function, if your driver needs/wants that.
 *
 * PARAMETERS
 *     Message - Format string; Works the same as printf().
 *     Arguments - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPrintSimpleVariadic(const char *Message, va_list Arguments) {
    KeIrql OldIrql = AcquireSpinLock();

    FlushY = VidpCursorY;
    __vprintf(Message, Arguments, NULL, PutBuffer);

    if (!PendingFullFlush) {
        FlushLines = VidpCursorY - FlushY + VidpFont.Height;
    }

    Flush();
    ReleaseSpinLock(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a non-prefixed message into the screen (we're the kernel equivalent
 *     of printf).
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPrintSimple(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Message);
    VidPrintSimpleVariadic(Message, Arguments);
    va_end(Arguments);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a prefixed message (in the format <Subsystem> <Type>: <Message>) into
 *     the screen; For most cases, you probably want VidPrint instead of this, but you can use this
 *     to "rename" the VidPrint function, if your driver needs/wants that.
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
        case VID_MESSAGE_TRACE:
            if (!VID_ENABLE_MESSAGE_TRACE) {
                return;
            }

            break;
        case VID_MESSAGE_DEBUG:
            if (!VID_ENABLE_MESSAGE_DEBUG) {
                return;
            }

            break;
        case VID_MESSAGE_INFO:
            if (!VID_ENABLE_MESSAGE_INFO) {
                return;
            }

            break;
    }

    KeIrql OldIrql = AcquireSpinLock();
    uint32_t OriginalForeground = VidpForeground;

    const char *Suffix;
    switch (Type) {
        case VID_MESSAGE_ERROR:
            VidpForeground = 0xFF0000;
            Suffix = " Error: ";
            break;
        case VID_MESSAGE_TRACE:
            VidpForeground = 0x00FF00;
            Suffix = " Trace: ";
            break;
        case VID_MESSAGE_DEBUG:
            VidpForeground = 0xFFFF00;
            Suffix = " Debug: ";
            break;
        default:
            VidpForeground = 0x0000FF;
            Suffix = " Info: ";
            break;
    }

    FlushY = VidpCursorY;
    PutString(Prefix);
    PutString(Suffix);
    VidpForeground = OriginalForeground;
    __vprintf(Message, Arguments, NULL, PutBuffer);

    if (!PendingFullFlush) {
        FlushLines = VidpCursorY - FlushY + VidpFont.Height;
    }

    Flush();
    ReleaseSpinLock(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a prefixed message (in the format <Subsystem> <Type>: <Message>) into
 *     the screen.
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
    va_start(Arguments, Message);
    VidPrintVariadic(Type, Prefix, Message, Arguments);
    va_end(Arguments);
}
