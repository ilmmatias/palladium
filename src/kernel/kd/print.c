/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kd.h>
#include <kernel/kdp.h>
#include <kernel/ke.h>
#include <kernel/vidp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
    /* Don't bother if neither the display nor debugger sinks are available. */
    if (!KdpDebugEchoEnabled && !KdpDebugConnected) {
        return;
    }

    /* Also don't bother if this type of message is disabled. */
    if ((Type == KD_TYPE_TRACE && !KD_ENABLE_TRACE) ||
        (Type == KD_TYPE_DEBUG && !KD_ENABLE_DEBUG)) {
        return;
    }

    /* Lock any other processors from messing with our buffer while we do it. */
    bool Locked = UseLock;
    KeIrql OldIrql = 0;
    if (Locked) {
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
    const char *Prefix = NULL;
    if (Type == KD_TYPE_ERROR) {
        Prefix = "* error: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0xFF0000;
        }
    } else if (Type == KD_TYPE_TRACE) {
        Prefix = "* trace: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0x00FF00;
        }
    } else if (Type == KD_TYPE_DEBUG) {
        Prefix = "* debug: ";
        if (KdpDebugEchoEnabled) {
            VidpForeground = 0xFFFF00;
        }
    } else if (Type == KD_TYPE_INFO) {
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

    /* And the main message on gray-white foreground + black background. */
    int FormattedSize = vsnprintf(Buffer, sizeof(Buffer), Message, Arguments);
    int Size = 0;
    if (FormattedSize > 0) {
        size_t Available = sizeof(Buffer);
        Size = (size_t)FormattedSize < Available ? FormattedSize : Available - 1;
    }

    if (KdpDebugEchoEnabled) {
        /* The start of the buffer contains the header + color prefix for the debugger, but our
         * PutString function can't handle that, so ignore the initial characters. */
        VidpForeground = 0xAAAAAA;
        VidpPutString(Buffer);

        /* Then restore everything (and flush the screen), and we're done on the echoing side. */
        VidpBackground = OldBackground;
        VidpForeground = OldForeground;

        if (!VidpPendingFullFlush) {
            VidpFlushLines = VidpCursorY - VidpFlushY + VidpFont.Height;
        }

        VidpFlush();
        VidpReleaseSpinLock(KE_IRQL_DISPATCH);
    }

    /* The protocol carries severity separately from UTF-8 text; frontends own any ANSI styling. */
    if (KdpDebugConnected) {
        uint8_t Payload[KDP_PROTOCOL_MAX_PAYLOAD];
        KdpProtocolOutput *Output = (void *)Payload;
        size_t OutputSize = Size;
        if (OutputSize > sizeof(Payload) - sizeof(*Output)) {
            OutputSize = sizeof(Payload) - sizeof(*Output);
        }
        Output->Severity = Type;
        Output->Length = OutputSize;
        memcpy(Output + 1, Buffer, OutputSize);
        KdpSendProtocolPacket(
            KDP_PROTOCOL_TARGET_OUTPUT,
            KDP_PROTOCOL_FLAG_EVENT,
            KDP_PROTOCOL_STATUS_SUCCESS,
            0,
            Payload,
            sizeof(*Output) + OutputSize);
    }

    if (Locked) {
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
