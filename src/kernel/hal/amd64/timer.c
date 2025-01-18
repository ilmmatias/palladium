/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the per-CPU event timer (Local APIC Timer).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeApicTimer(void) {
    /* Max out the divider. */
    HalpWriteLapicRegister(0x3E0, 0);

    /* We'll be taking the average over 4 runs. */
    uint64_t Accum = 0;
    uint64_t Ticks = (1 * EV_MILLISECS) / HalGetTimerPeriod();
    for (int i = 0; i < 4; i++) {
        uint64_t End = HalGetTimerTicks() + Ticks;
        HalpWriteLapicRegister(0x380, UINT32_MAX);
        while (HalGetTimerTicks() < End)
            ;

        HalpWriteLapicRegister(0x320, 0x10000);
        Accum += UINT32_MAX - HalpReadLapicRegister(0x390);
    }

    /* Periodic timer, trigerring IRQ 32 (our clock handler). */
    HalpWriteLapicRegister(0x320, 0x20040);
    HalpWriteLapicRegister(0x3E0, 0);
    HalpWriteLapicRegister(0x380, Accum / 4);
}
