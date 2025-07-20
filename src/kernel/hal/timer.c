/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/ev.h>
#include <kernel/hal.h>

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
    uint64_t Start = HalGetTimerTicks();
    uint64_t End = Start + (__uint128_t)Time * HalGetTimerFrequency() / EV_SECS;
    while (HalGetTimerTicks() < End)
        ;
}
