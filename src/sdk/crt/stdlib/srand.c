/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl.h>
#include <stdint.h>
#include <stdlib.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets the current state of the pseudo random number generator, with reduced
 *     precision.
 *
 * PARAMETERS:
 *     seed - New seed value; 0 is invalid for us, and 1 is used instead.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void srand(unsigned seed) {
    __srand64(seed);
}
