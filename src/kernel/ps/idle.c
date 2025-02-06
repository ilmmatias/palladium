/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function executes when a processor has no threads to execute.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void PspIdleThread(void *) {
    /* This value should always be the same (because the idle thread can't be moved between
     * processors). */
    KeProcessor *Processor = KeGetCurrentProcessor();

    while (true) {
        PauseProcessor();

        /* If we have a new thread to execute, get a dispatch interrupt to come up asap. */
        if (Processor->ThreadQueue.Next != &Processor->ThreadQueue) {
            HalpNotifyProcessor(Processor);
        }
    }
}
