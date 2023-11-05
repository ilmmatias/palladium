/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <hal.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function does a busy wait loop waiting until the specified amount of nanoseconds
 *     passed.
 *
 * PARAMETERS:
 *     Time - How many nanoseconds to wait.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalWaitTimer(uint64_t Time) {
    uint64_t End = HalGetTimerTicks() + Time / HalGetTimerPeriod();
    while (HalGetTimerTicks() < End)
        ;
}
