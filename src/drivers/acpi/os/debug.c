/* SPDX-FileCopyrightText: (C) 2025-2026 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <kernel/kd.h>
#include <kernel/ke.h>
#include <stdarg.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a info (basic debug) message to the screen if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowInfoMessage(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Message);
    KdPrintVariadic(KD_TYPE_INFO, Message, Arguments);
    va_end(Arguments);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a misc debug message to the screen if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowDebugMessage(const char *Message, ...) {
    if (KD_ENABLE_DEBUG) {
        va_list Arguments;
        va_start(Arguments, Message);
        KdPrintVariadic(KD_TYPE_DEBUG, Message, Arguments);
        va_end(Arguments);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows a tracing (I/O related and device initialization) message to the screen
 *     if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipShowTraceMessage(const char *Message, ...) {
    if (KD_ENABLE_TRACE) {
        va_list Arguments;
        va_start(Arguments, Message);
        KdPrintVariadic(KD_TYPE_TRACE, Message, Arguments);
        va_end(Arguments);
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function halts the system with the given reason, printing a debug message to the
 *     screen if possible.
 *
 * PARAMETERS:
 *     Reason - What went wrong.
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void AcpipShowErrorMessage(int Reason, const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Message);
    KdPrintVariadic(KD_TYPE_ERROR, Message, Arguments);
    va_end(Arguments);
    KeFatalError(
        KE_PANIC_DRIVER_INITIALIZATION_FAILURE,
        KE_PANIC_PARAMETER_ACPI_INITIALIZATION_FAILURE,
        Reason,
        0,
        0);
}
