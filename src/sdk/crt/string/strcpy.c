#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy a C string to another buffer.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strcpy(char *dest, const char *src) {
    char *Destination = dest;
    const char *Source = src;

    while (*Source) {
        *(Destination++) = *(Source++);
    }

    *Destination = 0;

    return dest;
}
