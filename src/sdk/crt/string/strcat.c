#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to concatenate a C string at the end of another.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strcat(char *dest, const char *src) {
    char *Destination = dest;
    const char *Source = src;

    while (*Destination) {
        Destination++;
    }

    while (*Source) {
        *(Destination++) = *(Source++);
    }

    *Destination = 0;

    return dest;
}
