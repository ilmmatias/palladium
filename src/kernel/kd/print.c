/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/kdp.h>
#include <kernel/ke.h>
#include <kernel/vidp.h>
#include <stdio.h>

extern const VidpFontData VidpFont;

extern bool VidpPendingFullFlush;
extern uint16_t VidpFlushY;
extern uint16_t VidpFlushLines;

extern uint32_t VidpBackground;
extern uint32_t VidpForeground;
extern uint16_t VidpCursorY;

extern bool KdpDebugConnected;

extern uint16_t KdpDebuggeePort;

extern uint8_t KdpDebuggerHardwareAddress[6];
extern uint8_t KdpDebuggerProtocolAddress[4];
extern uint16_t KdpDebuggerPort;

static KeSpinLock Lock = {0};
static char Buffer[512] = {0};

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
    /* Don't bother if neither echoing is enabled nor the debugger is connected. */
    if (!KD_ECHO_ENABLED && !KdpDebugConnected) {
        return;
    }

    /* Lock any other processors from messing with our buffer while we do it. */
    KeIrql OldIrql = KeAcquireSpinLockAndRaiseIrql(&Lock, KE_IRQL_DISPATCH);

    /* If echoing is enabled, we're going to mess with the display first (to print the prefix
     * using the right colors). */
    uint32_t OldBackground = 0;
    uint32_t OldForeground = 0;
    if (KD_ECHO_ENABLED) {
        VidpAcquireSpinLock();
        OldBackground = VidpBackground;
        OldForeground = VidpForeground;
    }

    /* Extract out the prefix string + color out of the type. */
    const char *ColorPrefix = NULL;
    const char *Prefix = NULL;
    if (Type == KD_TYPE_ERROR) {
        ColorPrefix = "\033[38;5;196m";
        Prefix = "* error: ";
        if (KD_ECHO_ENABLED) {
            VidpForeground = 0xFF0000;
        }
    } else if (Type == KD_TYPE_TRACE) {
        ColorPrefix = "\033[38;5;46m";
        Prefix = "* trace: ";
        if (KD_ECHO_ENABLED) {
            VidpForeground = 0x00FF00;
        }
    } else if (Type == KD_TYPE_DEBUG) {
        ColorPrefix = "\033[38;5;226m";
        Prefix = "* debug: ";
        if (KD_ECHO_ENABLED) {
            VidpForeground = 0xFFFF00;
        }
    } else {
        ColorPrefix = "\033[38;5;51m";
        Prefix = "* info: ";
        if (KD_ECHO_ENABLED) {
            VidpForeground = 0x00FFFF;
        }
    }

    /* Print the prefix on the correct color + black background. */
    if (KD_ECHO_ENABLED) {
        VidpBackground = 0x000000;
        VidpFlushY = VidpCursorY;
        VidpPutString(Prefix);
    }

    /* And the main message on gray-white foreground + black background. */
    int Offset = sizeof(KdpDebugPacket);
    Offset +=
        snprintf(Buffer + Offset, sizeof(Buffer) - Offset, "%s%s\033[0m", ColorPrefix, Prefix);
    int Size = vsnprintf(Buffer + Offset, sizeof(Buffer) - Offset, Message, Arguments);

    if (KD_ECHO_ENABLED) {
        /* The start of the buffer contains the header + color prefix for the debugger, but our
         * PutString function can't handle that, so ignore the initial characters. */
        VidpForeground = 0xAAAAAA;
        VidpPutString(Buffer + Offset);

        /* Then restore everything (and flush the screen), and we're done on the echoing side. */
        VidpBackground = OldBackground;
        VidpForeground = OldForeground;

        if (!VidpPendingFullFlush) {
            VidpFlushLines = VidpCursorY - VidpFlushY + VidpFont.Height;
        }

        VidpFlush();
        VidpReleaseSpinLock(KE_IRQL_DISPATCH);
    }

    /* We still do need to handle sending the message to the debugger; We already appended the
     * prefix (with the right ANSI color code), so we can just mount and send the debugger
     * packet (inside the buffer start, as we also reserved some space for the header there). */
    if (KdpDebugConnected) {
        KdpDebugPacket *Packet = (KdpDebugPacket *)Buffer;
        Packet->Type = KDP_DEBUG_PACKET_PRINT;
        KdpSendUdpPacket(
            KdpDebuggerHardwareAddress,
            KdpDebuggerProtocolAddress,
            KdpDebuggeePort,
            KdpDebuggerPort,
            Buffer,
            Offset + Size + 1);
    }

    KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a message into the debugger (echoing it back into the screen if
 *     enabled to do so).
 *
 * PARAMETERS:
 *     Type - Which kind of message this is (this defines the prefix we prepend to the message).
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
