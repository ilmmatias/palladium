/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ke.h>
#include <vid.h>

#include <sdt.hxx>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function searches for a specific table inside the RSDT/XSDT.
 *
 * PARAMETERS:
 *     Signature - Signature of the required entry.
 *
 * RETURN VALUE:
 *     Pointer to the header of the entry, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
SdtHeader *AcpipFindTable(const char Signature[4], int Index) {
    return (SdtHeader *)KiFindAcpiTable(Signature, Index);
}

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
    VidPrintVariadic(VID_MESSAGE_INFO, "ACPI Driver", Message, Arguments);
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
    va_list Arguments;
    va_start(Arguments, Message);
    VidPrintVariadic(VID_MESSAGE_DEBUG, "ACPI Driver", Message, Arguments);
    va_end(Arguments);
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
    va_list Arguments;
    va_start(Arguments, Message);
    VidPrintVariadic(VID_MESSAGE_TRACE, "ACPI Driver", Message, Arguments);
    va_end(Arguments);
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
    VidPrintVariadic(VID_MESSAGE_ERROR, "ACPI Driver", Message, Arguments);
    va_end(Arguments);
    KeFatalError(Reason);
}
