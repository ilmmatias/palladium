/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/fmt.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *buffer;
    size_t bufsz;
} context_t;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles writting the buffer into the given string; It doesn't handle
 *     null-terminating anything, vsnprintf() should do that!
 *
 * PARAMETERS:
 *     buffer - Input (character) buffer.
 *     size - Input buffer size.
 *     context - Expected to be a pointer to where we store the current position along the string,
 *               along how many bytes we have left in the output string.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static void put_buf(const void *buffer, int size, void *context) {
    context_t *str_context = context;

    if ((intmax_t)str_context->bufsz < size) {
        size = str_context->bufsz;
    }

    if (size > 0) {
        memcpy(str_context->buffer, buffer, size);
        str_context->buffer += size;
        str_context->bufsz -= size;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function outputs formatted text into a string.
 *     While snprintf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     bufsz - Size of the output buffer.
 *     format - Base format string.
 *     arg - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int vsnprintf(char *CRT_RESTRICT s, size_t bufsz, const char *CRT_RESTRICT format, va_list arg) {
    context_t context;
    context.buffer = s;
    context.bufsz = bufsz > 0 ? bufsz - 1 : 0;

    int size = __vprintf(format, arg, &context, put_buf);
    if (bufsz > 0) {
        *context.buffer = 0;
    }

    return size;
}
