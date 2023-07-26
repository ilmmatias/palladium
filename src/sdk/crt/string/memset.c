#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to fill a region of memory with a specific byte.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     c - Byte to be used as pattern.
 *     count - Size of the destination buffer.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memset(void *dest, int c, size_t count) {
    char *Destination = dest;

    while (count--) {
        *(Destination++) = c;
    }

    return dest;
}
