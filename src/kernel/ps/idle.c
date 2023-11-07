/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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
    while (1)
        ;
}
