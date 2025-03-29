/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/common.h>
#include <stddef.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements generic code to copy bytes from one region of memory to another.
 *
 * PARAMETERS:
 *     s1 - Destination buffer.
 *     s2 - Source buffer.
 *     n - How many bytes to copy.
 *
 * RETURN VALUE:
 *     Start of the destination buffer.
 *-----------------------------------------------------------------------------------------------*/
void *memcpy(void *CRT_RESTRICT s1, const void *CRT_RESTRICT s2, size_t n) {
    char *CRT_RESTRICT Destination = s1;
    const char *CRT_RESTRICT Source = s2;

    while (n--) {
        *(Destination++) = *(Source++);
    }

    return s1;
}
