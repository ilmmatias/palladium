/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kd.h>
#include <kernel/vidp.h>
#include <stdio.h>

extern const VidpFontData VidpFont;

extern bool VidpPendingFullFlush;
extern uint16_t VidpFlushY;
extern uint16_t VidpFlushLines;

extern uint32_t VidpBackground;
extern uint32_t VidpForeground;
extern uint16_t VidpCursorY;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the debugger (echoing it back into the screen if
 *     enabled to do so). You probably want KdPrint instead of this.
 *
 * PARAMETERS:
 *     Type - Which kind of message this is (this defines the prefix we prepend to the message).
 *     Message - Format string; Works the same as printf().
 *     Arguments - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdPrintVariadic(int Type, const char *Message, va_list Arguments) {
    /* TODO: Add support for an actual debugger; For now (as we only support echoing to the screen),
     * ignore any requests when echo is disabled. */
    if (!KD_ECHO_ENABLED) {
        return;
    }

    /* Acquire the lock before messing with the display's attributes. */
    KeIrql OldIrql = VidpAcquireSpinLock();
    uint32_t OldBackground = VidpBackground;
    uint32_t OldForeground = VidpForeground;

    /* Extract out the prefix string + color out of the type. */
    const char *Prefix = NULL;
    if (Type == KD_TYPE_ERROR) {
        Prefix = "* error: ";
        VidpForeground = 0xFF0000;
    } else if (Type == KD_TYPE_TRACE) {
        Prefix = "* trace: ";
        VidpForeground = 0x00FF00;
    } else if (Type == KD_TYPE_DEBUG) {
        Prefix = "* debug: ";
        VidpForeground = 0xFFFF00;
    } else {
        Prefix = "* info: ";
        VidpForeground = 0x00FFFF;
    }

    /* Print the prefix on the correct color + black background. */
    VidpBackground = 0x000000;
    VidpFlushY = VidpCursorY;
    VidpPutString(Prefix);

    /* And the main message on gray-white foreground + black background. */
    char Buffer[512];
    vsnprintf(Buffer, sizeof(Buffer), Message, Arguments);
    VidpForeground = 0xAAAAAA;
    VidpPutString(Buffer);

    /* Then restore everything and we're done. */
    VidpBackground = OldBackground;
    VidpForeground = OldForeground;

    if (!VidpPendingFullFlush) {
        VidpFlushLines = VidpCursorY - VidpFlushY + VidpFont.Height;
    }

    VidpFlush();
    VidpReleaseSpinLock(OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the debugger (echoing it back into the screen if
 *     enabled to do so).
 *
 * PARAMETERS:
 *     Type - Which kind of message this is ( his defines the prefix we prepend to the message).
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdPrint(int Type, const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Message);
    KdPrintVariadic(Type, Message, Arguments);
    va_end(Arguments);
}
