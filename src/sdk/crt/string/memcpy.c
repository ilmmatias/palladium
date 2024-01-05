/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy bytes from one region of memory to another.
 *
 * PARAMETERS:
 *     dest - Destination buffer.
 *     src - Source buffer.
 *     count - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memcpy(void *dest, const void *src, size_t count) {
#ifdef ARCH_amd64
    void *res = dest;
    __asm__ volatile("cld; rep movsb"
                     : "=D"(dest), "=S"(src), "=c"(count)
                     : "0"(dest), "1"(src), "c"(count)
                     : "flags", "memory");
    return res;
#else
    char *Destination = dest;
    const char *Source = src;

    while (count--) {
        *(Destination++) = *(Source++);
    }

    return dest;
#endif
}
