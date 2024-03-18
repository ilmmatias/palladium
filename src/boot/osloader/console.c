/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <efi/spec.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function displays a character to the screen.
 *
 * PARAMETERS:
 *     Character - The character.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslPutChar(char Character) {
    CHAR16 String[2] = {Character, 0};
    gST->ConOut->OutputString(gST->ConOut, String);
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
void OslPutString(const char *String) {
    size_t Size = strlen(String);
    CHAR16 WideString[Size + 1];

    for (size_t i = 0; i < Size; i++) {
        WideString[i] = String[i];
    }

    WideString[Size] = 0;
    gST->ConOut->OutputString(gST->ConOut, WideString);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     Wrapper around ConOut->OutputString for __vprint.
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
static void PutBuffer(const void *Buffer, int Size, void *) {
    const char *String = Buffer;
    CHAR16 WideString[Size + 1];

    for (int i = 0; i < Size; i++) {
        WideString[i] = *(String++);
    }

    WideString[Size] = 0;
    gST->ConOut->OutputString(gST->ConOut, WideString);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     printf() for the boot manager; Calling printf() will result in a linker error, use this
 *     instead!
 *
 * PARAMETERS:
 *     Message - Format string; Works the same as printf().
 *     ... - Variadic arguments.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslPrint(const char *Format, ...) {
    va_list vlist;
    va_start(vlist, Format);
    __vprintf(Format, vlist, NULL, PutBuffer);
    va_end(vlist);
}
