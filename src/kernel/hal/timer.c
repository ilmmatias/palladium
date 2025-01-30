/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <hal.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if we already reached or went past the given target timestamp.
 *     Use this instead of manually checking, as we also handle overflow on 32-bit timers.
 *
 * PARAMETERS:
 *     Current - Current value of HalGetTimerTicks().
 *     Reference - Value of HalGetTimerTicks() in the initial call time (for overflow checks).
 *     Time - How much timer ticks past the reference we want to wait until expiration.
 *
 * RETURN VALUE:
 *     true if we reached the target time, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool HalCheckTimerExpiration(uint64_t Current, uint64_t Reference, uint64_t Ticks) {
    uint64_t Target = Reference + Ticks;

    if (HalGetTimerWidth() == HAL_TIMER_WIDTH_64B) {
        /* 64-bit timers are easier to handle, we should be able to just not care about overflow. */
        return Current >= Target;
    }

    /* For 32-bit timers, we need to handle overflow; If the target time is still a 32-bit
     * number, then any overflow means we reached it; If it's 64-bits, then we need to wait til
     * overflow, and then mask off the high bits (and wait again). */
    if (Target <= UINT32_MAX) {
        return Current < Reference || Current >= Target;
    }

    return Current < Reference && Current >= ((Target >> 32) << 32);
}

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
    uint64_t Ticks = Time / HalGetTimerPeriod();
    while (!HalCheckTimerExpiration(HalGetTimerTicks(), Start, Ticks))
        ;
}
