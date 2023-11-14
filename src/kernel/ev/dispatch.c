/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ev.h>
#include <hal.h>
#include <psp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles any pending kernel events.
 *
 * PARAMETERS:
 *     Context - Current processor state.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void EvpHandleEvents(HalRegisterState *Context) {
    HalProcessor *Processor = HalGetCurrentProcessor();

    /* Process any pending DPCs. */
    while (Processor->DpcQueue.Next != &Processor->DpcQueue) {
        EvDpc *Dpc = CONTAINING_RECORD(RtPopDList(&Processor->DpcQueue), EvDpc, ListHeader);
        Dpc->Routine(Dpc->Context);
    }

    /* Wrap up by calling the scheduler; It should handle initialization+retriggering the event
       handler if required. */
    PspScheduleNext(Context);
}
