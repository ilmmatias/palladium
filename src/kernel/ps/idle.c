/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/intrin.h>

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
    while (true) {
        StopProcessor();
    }
}
