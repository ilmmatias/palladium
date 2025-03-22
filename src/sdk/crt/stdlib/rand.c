/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <crt_impl/rand.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function implements a pseudo random number generator (high 32-bits of the state,
 *     maintains the 2^64-1 period).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pseudo random data.
 *-----------------------------------------------------------------------------------------------*/
int rand(void) {
    return __rand64() >> 32;
}
