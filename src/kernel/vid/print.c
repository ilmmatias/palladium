/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/fmt.h>
#include <kernel/ke.h>
#include <kernel/vidp.h>
#include <string.h>

extern const VidpFontData VidpFont;

bool VidpPendingFullFlush = false;
uint16_t VidpFlushY = 0;
uint16_t VidpFlushLines = 0;

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
KeIrql VidpAcquireSpinLock(void) {
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
void VidpReleaseSpinLock(KeIrql OldIrql) {
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
void VidpFlush(void) {
    memcpy(
        VidpBackBuffer + VidpFlushY * VidpPitch,
        VidpFrontBuffer + VidpFlushY * VidpPitch,
        VidpFlushLines * VidpPitch);
    VidpPendingFullFlush = false;
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

    VidpPendingFullFlush = true;
    VidpFlushY = 0;
    VidpFlushLines = VidpHeight;
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
    KeIrql OldIrql = VidpAcquireSpinLock();
    VidpCursorX = 0;
    VidpCursorY = 0;

    for (int i = 0; i < VidpHeight; i++) {
        for (int j = 0; j < VidpWidth; j++) {
            *(uint32_t *)&VidpFrontBuffer[i * VidpPitch + j * 4] = VidpBackground;
        }
    }

    VidpFlushY = 0;
    VidpFlushLines = VidpHeight;
    VidpFlush();
    VidpReleaseSpinLock(OldIrql);
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
void VidpPutChar(char Character) {
    /* It's probably safe to use the size of 4 spaces? */
    const int TabSize = VidpFont.GlyphInfo[0x20].Width * 4;

    if (VidpCursorX + VidpFont.GlyphInfo[(int)Character].Width > VidpWidth) {
        VidpCursorX = 0;
        VidpCursorY += VidpFont.Height;
    }

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
void VidpPutString(const char *String) {
    while (*String) {
        VidpPutChar(*(String++));
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
    KeIrql OldIrql = VidpAcquireSpinLock();

    VidpFlushY = VidpCursorY;
    VidpPutChar(Character);

    if (!VidpPendingFullFlush) {
        VidpFlushLines = VidpCursorY - VidpFlushY + VidpFont.Height;
    }

    VidpFlush();
    VidpReleaseSpinLock(OldIrql);
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
    KeIrql OldIrql = VidpAcquireSpinLock();

    VidpFlushY = VidpCursorY;
    VidpPutString(String);

    if (!VidpPendingFullFlush) {
        VidpFlushLines = VidpCursorY - VidpFlushY + VidpFont.Height;
    }

    VidpFlush();
    VidpReleaseSpinLock(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the screen (we're the kernel equivalent of vprintf).
 *     For most cases, you probably want VidPrint instead of this, but you can use this to
 *     "rename" the VidPrint function, if your driver needs/wants that.
 *
 * PARAMETERS
 *     Message - Format string; Works the same as printf().
 *     Arguments - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPrintVariadic(const char *Message, va_list Arguments) {
    char Buffer[512];
    vsnprintf(Buffer, sizeof(Buffer), Message, Arguments);
    VidPutString(Buffer);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the screen (we're the kernel equivalent of printf).
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void VidPrint(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Message);
    VidPrintVariadic(Message, Arguments);
    va_end(Arguments);
}
