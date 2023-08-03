#include <stdarg.h>
#include <stdio.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a standard complying version of `formatting into stdout`.
 *     For supported format parameters, take a look at your favorite std C reference manual.
 *
 * PARAMETERS:
 *     format - Base format string.
 *     ... - Further variadic arguments.
 *
 * RETURN VALUE:
 *     How many characters have been output.
 *-----------------------------------------------------------------------------------------------*/
int printf(const char *format, ...) {
    va_list vlist;
    va_start(vlist, format);
    int size = vprintf(format, vlist);
    va_end(vlist);
    return size;
}
