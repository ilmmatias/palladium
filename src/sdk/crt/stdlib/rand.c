/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <crt_impl.h>

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
