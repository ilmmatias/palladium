/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <acpip.h>
#include <kernel/ev.h>
#include <kernel/hal.h>
#include <kernel/ps.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the current system timer value in 100-nanosecond units.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Current timer value in 100ns units.
 *-----------------------------------------------------------------------------------------------*/
uint64_t AcpipGetTimer(void) {
    return (__uint128_t)HalGetTimerTicks() * (EV_SECS / 100) / HalGetTimerFrequency();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function suspends the current thread for the specified number of milliseconds.
 *
 * PARAMETERS:
 *     Milliseconds - How long to sleep.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipDelayThread(uint64_t Milliseconds) {
    PsDelayThread(Milliseconds * EV_MILLISECS);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function performs a busy-wait for the specified number of microseconds.
 *
 * PARAMETERS:
 *     Microseconds - How long to stall (should be <= 100, according to the ACPI specs).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void AcpipStallExecution(uint64_t Microseconds) {
    HalWaitTimer(Microseconds * EV_MICROSECS);
}
