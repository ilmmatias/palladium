/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <stdlib.h>

uint64_t __rand_state = 1;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a pseudo random number generator (32-bits wide, 2^64-1 period).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pseudo random data.
 *-----------------------------------------------------------------------------------------------*/
int rand(void) {
    /* xorshift64star, same implementation as https://en.wikipedia.org/wiki/Xorshift, returning
       only the high 32-bits. */
    __rand_state ^= __rand_state >> 12;
    __rand_state ^= __rand_state << 25;
    __rand_state ^= __rand_state >> 27;
    return (__rand_state * 0x2545F4914F6CDD1D) >> 32;
}
