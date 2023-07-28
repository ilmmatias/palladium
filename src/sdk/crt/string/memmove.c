#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to safely copy bytes from one region of memory to
 *     itself.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     count - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memmove(void *dest, const void *src, size_t count) {
    char *Destination = (char *)dest + count;
    const char *Source = (const char *)src + count;

    while (count--) {
        *(--Destination) = *(--Source);
    }

    return dest;
}
