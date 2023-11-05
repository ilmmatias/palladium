/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <pcip.h>
#include <vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function shows an info message to the screen if allowed.
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PcipShowInfoMessage(const char *Message, ...) {
    va_list Arguments;
    va_start(Arguments, Format);
    VidPrintVariadic(KE_MESSAGE_INFO, "PCI Driver", Message, Arguments);
    va_end(Arguments);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function halts the system with the given reason, printing a debug message to the
 *     screen if possible.
 *
 * PARAMETERS:
 *     Code - What went wrong.
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void PcipShowErrorMessage(int Code, const char *Message, ...) {
    va_list Arguments;

    va_start(Arguments, Format);
    VidPrintVariadic(KE_MESSAGE_ERROR, "PCI Driver", Message, Arguments);
    va_end(Arguments);

    KeFatalError(Code);
}
