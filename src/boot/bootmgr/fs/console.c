/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>
#include <ctype.h>
#include <display.h>
#include <file.h>
#include <keyboard.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_SIZE 128

typedef struct {
    char Line[LINE_SIZE];
    size_t Position;
    size_t Size;
} ConsoleContext;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function parses the given path segment, returning a read-write console device on
 *     match.
 *
 * PARAMETERS:
 *     Segment - Path segment string.
 *     Context - Output; Just the `Type` field matters to us.
 *
 * RETURN VALUE:
 *     How many characters we consumed if there was a path match, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiOpenConsoleDevice(const char *Segment, FileContext *Context) {
    /* console()/ (mind the parenthesis, they are required); We don't have any
       subdirectories/files, so whatever comes after the slash doesn't matter. */
    if (tolower(*Segment) != 'c' || tolower(*(Segment + 1)) != 'o' ||
        tolower(*(Segment + 2)) != 'n' || tolower(*(Segment + 3)) != 's' ||
        tolower(*(Segment + 4)) != 'o' || tolower(*(Segment + 5)) != 'l' ||
        tolower(*(Segment + 6)) != 'e' || tolower(*(Segment + 7)) != '(' ||
        tolower(*(Segment + 8)) != ')') {
        return 0;
    }

    Context->Type = FILE_TYPE_CONSOLE;
    Context->PrivateSize = sizeof(ConsoleContext);
    Context->PrivateData = calloc(1, sizeof(ConsoleContext));

    return Context->PrivateData ? 9 : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function reads a single character from the keyboard.
 *
 * PARAMETERS:
 *     Context - Device private data.
 *     Buffer - Output buffer.
 *     Start - Starting byte index (ignored by us).
 *     Size - How many characters to read into the buffer (ignored by us).
 *     Read - How many characters we read with no error.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiReadConsoleDevice(
    FileContext *Context,
    void *Buffer,
    size_t Start,
    size_t Size,
    size_t *Read) {
    (void)Start;

    /* The CRT expects us to be buffering the input ourselves too (storing the whole line).
       So, we need to call PollKey until we reach the end of the line (ignoring any characters
       past our max line size). */

    ConsoleContext *DevContext = Context->PrivateData;

    if (!DevContext->Size) {
        DevContext->Position = 0;

        while (1) {
            int Key = BmPollKey();

            /* Special keys like ESC need manual handling via BmPollKey(). */
            if (Key >= KEY_ESC) {
                Key = KEY_UNKNOWN;
            }

            if (DevContext->Size < LINE_SIZE) {
                DevContext->Line[DevContext->Size++] = Key;
            }

            BmPutChar(Key);

            if (Key == '\n') {
                break;
            }
        }
    }

    size_t CopySize = DevContext->Size;
    if (CopySize > Size) {
        CopySize = Size;
    }

    memcpy(Buffer, DevContext->Line + DevContext->Position, CopySize);

    DevContext->Position += CopySize;
    DevContext->Size -= CopySize;

    if (Read) {
        *Read = CopySize;
    }

    return CopySize < Size ? __STDIO_FLAGS_EOF : 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function writes `Size` characters into the display.
 *
 * PARAMETERS:
 *     Context - Device private data.
 *     Buffer - Input buffer.
 *     Start - Starting byte index (ignored by us).
 *     Size - How many characters to write from the buffer.
 *     Read - How many characters we wrote with no error.
 *
 * RETURN VALUE:
 *     __STDIO_FLAGS_ERROR/EOF if something went wrong, 0 otherwise.
 *-----------------------------------------------------------------------------------------------*/
int BiWriteConsoleDevice(
    FileContext *Context,
    const void *Buffer,
    size_t Start,
    size_t Size,
    size_t *Wrote) {
    const char *InputBuffer = Buffer;

    (void)Context;
    (void)Start;

    for (size_t i = 0; i < Size; i++) {
        BmPutChar(*(InputBuffer++));
    }

    if (Wrote) {
        *Wrote = Size;
    }

    /* BmPutChar() never fails, so we don't have any error to return. */
    return 0;
}
