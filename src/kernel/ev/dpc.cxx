/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <ev.h>

#include <cxx/critical_section.hxx>

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
extern "C" void EvInitializeDpc(EvDpc *Dpc, void (*Routine)(void *Context), void *Context) {
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
extern "C" void EvDispatchDpc(EvDpc *Dpc) {
    CriticalSection Guard;
    RtAppendDList(&HalGetCurrentProcessor()->DpcQueue, &Dpc->ListHeader);
}
