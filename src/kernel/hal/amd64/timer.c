/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/evp.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/vid.h>

bool HalpTscActive = false;
bool HalpTimerInitialized = false;

static uint64_t Frequency = 0;
static uint64_t (*GetTicks)(void) = NULL;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the system timer, preferably using the option with the least
 *     access latency and highest frequency/accuracy.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeTimer(void) {
    const char* Source;
    if (HalpGetTscFrequency()) {
        /* Prefer the TSC if we have invariant TSC support (as it has a way lower latency for
         * access, and is guaranteed to be 64-bits). */
        HalpTscActive = true;
        Source = "TSC";
        Frequency = HalpGetTscFrequency();
        GetTicks = HalpGetTscTicks;
    } else {
        /* Otherwise, fallback to the HPET (which is slower, but also works). */
        Source = "HPET";
        Frequency = HalpGetHpetFrequency();
        GetTicks = HalpGetHpetTicks;
    }

    /* Print the frequency in MHz (we'd expect the performance counter to have at least a 1MHz
     * frequency; If it's less than that, it will still hopefully show up the fractional part
     * though). */
    VidPrint(
        VID_MESSAGE_INFO,
        "Kernel HAL",
        "using %s as the timer source (frequency = %llu.%02llu MHz)\n",
        Source,
        Frequency / 1000000,
        (Frequency % 1000000) / 10000);

    HalpTimerInitialized = true;
}

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

    /* We'll be taking the average over 5 runs. */
    uint64_t Accum = 0;
    uint64_t Ticks = (__uint128_t)EVP_TICK_PERIOD * HalGetTimerFrequency() / EV_SECS;
    for (int i = 0; i < 5; i++) {
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
    HalpWriteLapicRegister(HALP_APIC_TIMER_ICR_REG, Accum / 5);
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the frequency (in Hz) of the system timer.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Frequency of the timer.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalGetTimerFrequency(void) {
    return Frequency;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns how many timer ticks have elapsed since the system timer was
 *     initialized.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     How many ticks have elapsed since system boot. Multiply it by the timer period to get how
 *     many nanoseconds have elapsed.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalGetTimerTicks(void) {
    return GetTicks();
}
