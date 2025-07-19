/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/evp.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>

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
    HalpWriteLapicRegister(HALP_APIC_TIMER_DCR_REG, 0);

    /* We'll be taking the average over 4 runs. */
    uint64_t Accum = 0;
    uint64_t Ticks = ((__uint128_t)EVP_TICK_PERIOD * HalGetTimerFrequency()) / EV_SECS;
    for (int i = 0; i < 4; i++) {
        uint64_t End = HalGetTimerTicks() + Ticks;
        HalpWriteLapicRegister(HALP_APIC_TIMER_ICR_REG, UINT32_MAX);
        while (HalGetTimerTicks() < End)
            ;
        Accum += UINT32_MAX - HalpReadLapicRegister(HALP_APIC_TIMER_CCR_REG);
    }

    /* Now we can enable the LAPIC in periodic mode. */
    HalpApicLvtRecord Record = {0};
    Record.Vector = HALP_INT_TIMER_VECTOR;
    Record.Periodic = 1;
    HalpWriteLapicRegister(HALP_APIC_LVTT_REG, Record.RawData);
    HalpWriteLapicRegister(HALP_APIC_TIMER_DCR_REG, 0);
    HalpWriteLapicRegister(HALP_APIC_TIMER_ICR_REG, Accum / 4);
}
