/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifndef _AMD64_PAUSE_H_
#define _AMD64_PAUSE_H_

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals the CPU we're in a busy loop (waiting for a spin lock probably), and
 *     we are safe to drop the CPU state into power saving.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void PauseProcessor(void) {
    __asm__ volatile("pause");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals the CPU we're either fully done with any work this CPU will ever do
 *     (aka, panic), or that we're waiting some interrupt/external event.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
static inline void StopProcessor(void) {
    __asm__ volatile("hlt");
}

#endif /* _AMD64_PAUSE_H */
