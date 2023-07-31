#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to concatenate a C string at the end of another,
 *     while assuming that the destination buffer can store at most an extra `count` bytes.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     count - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
char *strncat(char *dest, const char *src, size_t count) {
    char *Destination = dest;
    const char *Source = src;

    while (*Destination) {
        Destination++;
    }

    while (*Source && count) {
        *(Destination++) = *(Source++);
        count--;
    }

    if (count) {
        *Destination = 0;
    }

    return dest;
}
