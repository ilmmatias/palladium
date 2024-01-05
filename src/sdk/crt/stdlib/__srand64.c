/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <stdlib.h>

extern uint64_t __rand_state;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the current state of the pseudo random number generator, with full
 *     precision.
 *
 * PARAMETERS:
 *     seed - New seed value; 0 is invalid for us, and 1 is used instead.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void __srand64(uint64_t seed) {
    __rand_state = seed ? seed : 1;
}
