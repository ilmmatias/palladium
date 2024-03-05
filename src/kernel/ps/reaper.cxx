/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <mm.h>
#include <psp.h>
#include <vid.h>

#include <ke.hxx>

extern "C" {
KeSpinLock PspReaperLock = {0};
RtDList PspReaperList;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function handles the cleanup of dead (terminated) threads, whenever possible.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
extern "C" [[noreturn]] void PspReaperThread(void *) {
    while (1) {
        /* Stall until we have something; We're of the lowest possible priority, so we should only
         * execute on idle (or if the scheduler thinks it's the right time to do so). */
        while (PspReaperList.Next == &PspReaperList) {
            HalGetCurrentProcessor()->ForceYield = PSP_YIELD_REQUEST;
            HalpSetEvent(0);
        }

        SpinLockGuard Guard(&PspReaperLock);
        while (PspReaperList.Next != &PspReaperList) {
            PsThread *Thread = CONTAINING_RECORD(RtPopDList(&PspReaperList), PsThread, ListHeader);
            MmFreePool(Thread->Stack, "Ps  ");
            MmFreePool(Thread, "Ps  ");
        }
    }
}
