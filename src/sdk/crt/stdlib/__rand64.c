/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <stdlib.h>

uint64_t __rand_state = 1;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a pseudo random number generator (2^64-1 period).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pseudo random data.
 *-----------------------------------------------------------------------------------------------*/
uint64_t __rand64(void) {
    /* xorshift64star, same implementation as https://en.wikipedia.org/wiki/Xorshift. */
    __rand_state ^= __rand_state >> 12;
    __rand_state ^= __rand_state << 25;
    __rand_state ^= __rand_state >> 27;
    return __rand_state * 0x2545F4914F6CDD1D;
}
