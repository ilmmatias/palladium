#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy bytes from one region of memory to another,
 *     stopping after copying the specified amount or after copying the specified byte.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     c - Marker.
 *     count - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Pointer to the byte after the marker, or NULL if the marker wasn't found.
 *-----------------------------------------------------------------------------------------------*/
void *memccpy(void *dest, const void *src, int c, size_t count) {
    char *Destination = dest;
    const char *Source = src;

    while (count) {
        int Data = *(Source++);
        *(Destination++) = Data;

        if (Data == c) {
            break;
        }

        count--;
    }

    return count ? Destination : NULL;
}
