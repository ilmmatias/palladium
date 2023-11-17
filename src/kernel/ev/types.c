/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ev.h>
#include <hal.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given timer event, setting its deadline relative to the
 *     current time.
 *
 * PARAMETERS:
 *     Timer - Pointer to the timer struct.
 *     Timeout - How many nanoseconds to sleep for; If this is set too low, EvWaitObject(s)
 *     Dpc - Optional DPC to be executed/enqueued when we sleep.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvInitializeTimer(EvTimer *Timer, uint64_t Timeout, EvDpc *Dpc) {
    Timer->Type = EV_TYPE_TIMER;
    Timer->Dpc = Dpc;
    Timer->Deadline = HalGetTimerTicks() + Timeout / HalGetTimerPeriod();
}
