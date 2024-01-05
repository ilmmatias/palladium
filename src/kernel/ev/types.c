/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <evp.h>
#include <string.h>

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
    memset(Timer, 0, sizeof(EvTimer));
    Timer->Type = EV_TYPE_TIMER;
    Timer->Dpc = Dpc;

    /* Dispatch the DPC if this is 0-timeout event, and don't bother with the event dispatcher
       (just set us as finished). */
    if (!Timeout) {
        Timer->Finished = 1;
        EvDispatchDpc(Dpc);
        return;
    }

    EvpDispatchObject(Timer, Timeout, 0);
}
