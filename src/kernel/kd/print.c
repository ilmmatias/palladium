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

extern bool KdpDebugEchoEnabled;
extern bool KdpDebugConnected;

extern uint16_t KdpDebuggeePort;

extern uint8_t KdpDebuggerHardwareAddress[6];
extern uint8_t KdpDebuggerProtocolAddress[4];
extern uint16_t KdpDebuggerPort;

static bool UseLock = true;
static bool PreviousEchoEnabled = true;
static KeSpinLock Lock = {0};
static char Buffer[1024] = {0};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function acquires the ownership of the debugger print buffer lock (essentially
 *     skipping Acquire/ReleaseSpinLock when calling KdPrintVariadic).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpAcquireOwnership(void) {
    UseLock = false;
    PreviousEchoEnabled = KdpDebugEchoEnabled;
    KdpDebugEchoEnabled = false;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function releases the ownership of the debugger print buffer lock (which makes
 *     KdPrintVariadic start calling Acquire/ReleaseSpinLock again).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KdpReleaseOwnership(void) {
    UseLock = true;
    KdpDebugEchoEnabled = PreviousEchoEnabled;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a prefixed message into the debugger and the screen (if enabled to do
 *     so). You probably want KdPrint instead of this.
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
    if (!KdpDebugEchoEnabled && !KdpDebugConnected) {
        return;
    }

    /* Lock any other processors from messing with our buffer while we do it. */
    KeIrql OldIrql;
    if (UseLock) {
        OldIrql = KeAcquireSpinLockAndRaiseIrql(&Lock, KE_IRQL_DISPATCH);
    }

    /* If echoing is enabled, we're going to mess with the display first (to print the prefix
     * using the right colors). */
    uint32_t OldBackground = 0;
    uint32_t OldForeground = 0;
    if (KdpDebugEchoEnabled) {
        VidpAcquireSpinLock();
        OldBackground = VidpBackground;
        OldForeground = VidpForeground;
    }

    /* Extract out the prefix string + color out of the type. */
    const char *ColorPrefix = NULL;
    const char *Prefix = NULL;
    if (Type == KD_TYPE_ERROR) {
        ColorPrefix = KDP_ANSI_FG_RED;
        Prefix = "* error: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0xFF0000;
        }
    } else if (Type == KD_TYPE_TRACE) {
        ColorPrefix = KDP_ANSI_FG_GREEN;
        Prefix = "* trace: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0x00FF00;
        }
    } else if (Type == KD_TYPE_DEBUG) {
        ColorPrefix = KDP_ANSI_FG_YELLOW;
        Prefix = "* debug: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0xFFFF00;
        }
    } else if (Type == KD_TYPE_INFO) {
        ColorPrefix = KDP_ANSI_FG_BLUE;
        Prefix = "* info: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0x00FFFF;
        }
    }

    /* Print the prefix on the correct color + black background. */
    if (KdpDebugEchoEnabled) {
        VidpBackground = 0x000000;
        VidpFlushY = VidpCursorY;
        if (Prefix) {
            VidpPutString(Prefix);
        }
    }

    int Offset = sizeof(KdpDebugPacket);
    if (ColorPrefix && Prefix) {
        Offset += snprintf(
            Buffer + Offset, sizeof(Buffer) - Offset, "%s%s" KDP_ANSI_RESET, ColorPrefix, Prefix);
    }

    /* And the main message on gray-white foreground + black background. */
    int Size = vsnprintf(Buffer + Offset, sizeof(Buffer) - Offset, Message, Arguments);

    if (KdpDebugEchoEnabled) {
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
            Offset + Size);
    }

    if (UseLock) {
        KeReleaseSpinLockAndLowerIrql(&Lock, OldIrql);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs a prefixed message into the debugger and the screen (if enabled to do
 *     so).
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
