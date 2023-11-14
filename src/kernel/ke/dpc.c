/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <halp.h>
#include <ke.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the given DPC context struct.
 *     Do not try manually initializing the struct, there's no guarantee its fields will stay the
 *     same across different kernel revisions!
 *
 * PARAMETERS:
 *     Dpc - DPC context struct.
 *     Routine - What should be executed.
 *     Context - Context for the routine.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeInitializeDpc(KeDpc *Dpc, void (*Routine)(void *Context), void *Context) {
    Dpc->Routine = Routine;
    Dpc->Context = Context;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function adds a previously initialized DPC to this processor's list.
 *
 * PARAMETERS:
 *     Dpc - DPC context struct.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KeEnqueueDpc(KeDpc *Dpc) {
    void *Context = HalpEnterCriticalSection();
    RtAppendDList(&HalGetCurrentProcessor()->DpcQueue, &Dpc->ListHeader);
    HalpLeaveCriticalSection(Context);

    /* We need to trigger an event if we have nothing in sight. */
    if (!HalGetCurrentProcessor()->CurrentThread->Expiration) {
        HalpSetEvent(0);
    }
}
