/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <ke.h>
#include <pcip.h>
#include <stdarg.h>
#include <vid.h>

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
 *     This function shows an info message to the screen if allowed
 *     (PCI_ENABLE_INFO set to 1).
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void PcipShowInfoMessage(const char *Format, ...) {
    if (PCI_ENABLE_INFO) {
        va_list vlist;
        va_start(vlist, Format);

        VidPutString("PCI Info: ");
        __vprintf(Format, vlist, NULL, PutBuffer);

        va_end(vlist);
    }
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
[[noreturn]] void PcipShowErrorMessage(int Code, const char *Format, ...) {
    va_list vlist;
    va_start(vlist, Format);

    VidPutString("PCI Error: ");
    __vprintf(Format, vlist, NULL, PutBuffer);

    va_end(vlist);

    KeFatalError(Code);
}
