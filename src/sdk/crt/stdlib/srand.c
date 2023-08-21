/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <stdint.h>
#include <stdlib.h>

extern uint64_t __rand_state;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the current state of the pseudo random number generator;
 *
 * PARAMETERS:
 *     seed - New seed value; 0 is invalid for us, and 1 is used instead.
 *
 * RETURN VALUE:
 *     None
 *-----------------------------------------------------------------------------------------------*/
void srand(unsigned seed) {
    __rand_state = seed ? seed : 1;
}
