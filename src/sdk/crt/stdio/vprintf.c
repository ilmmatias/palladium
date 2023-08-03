#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void __put_stdout(const void *buffer, int size, void *context);

int __vprintf(
    const char *format,
    va_list vlist,
    void *context,
    void (*put_buf)(const void *buffer, int size, void *context));

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
int vprintf(const char *format, va_list vlist) {
    return __vprintf(format, vlist, NULL, __put_stdout);
}
