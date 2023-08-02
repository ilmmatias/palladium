#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char *buffer;
} context_t;

int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles writting the buffer into the given string; It doesn't handle
 *     null-terminating anything, vsprintf() should do that!
 *
 * PARAMETERS:
 *     buffer - Input (character) buffer.
 *     size - Input buffer size.
 *     context - Expected to be a pointer to where we store the current position along the string.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
static void put_buf(const void *buffer, int size, void *context) {
    context_t *str_context = (context_t *)context;
    memcpy(str_context->buffer, buffer, size);
    str_context->buffer += size;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a standard complying version of `formatting into a string`.
 *     While sprintf() takes in variadic arguments and calls va_start(), we take in the va_list
 *     (result of va_start).
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     buffer - Output buffer.
 *     format - Base format string.
 *     vlist - Variadic argument list.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int vsprintf(char *buffer, const char *format, va_list vlist) {
    context_t context;
    context.buffer = buffer;
    int size = __vprintf(format, vlist, &context, put_buf);
    *context.buffer = 0;
    return size;
}
