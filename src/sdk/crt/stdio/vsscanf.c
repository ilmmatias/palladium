/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/fmt.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *buffer;
} context_t;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles reading the next character from the buffer.
 *
 * PARAMETERS:
 *     context - Expected to be a pointer to where we store the current position along the string.
 *
 * RETURN VALUE:
 *     Either the next character, or EOF if we reached the end of the string.
 *-----------------------------------------------------------------------------------------------*/
static int read_ch(void *context) {
    context_t *str_context = context;
    return *(str_context->buffer) ? *(str_context->buffer++) : EOF;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles rewinding the buffer; As we know exactly how __vscanf works, we can
 *     safely simply decrease the buffer pointer.
 *
 * PARAMETERS:
 *     context - Expected to be a pointer to where we store the current position along the string.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static void unread_ch(void *context, int ch) {
    if (ch != EOF) {
        ((context_t *)context)->buffer--;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function matches values from a string based on another format string.
 *     While sscanf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     s - Output buffer.
 *     format - Base format string.
 *     arg - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many arguments have been filled.
 *-----------------------------------------------------------------------------------------------*/
int vsscanf(const char *CRT_RESTRICT s, const char *CRT_RESTRICT format, va_list arg) {
    context_t context;
    context.buffer = s;
    return __vscanf(format, arg, &context, read_ch, unread_ch);
}
