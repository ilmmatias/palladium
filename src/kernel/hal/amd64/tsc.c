/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <cpuid.h>
#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/vid.h>
#include <stdint.h>

static uint64_t Frequency = 0;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to initialize the TSC as a timer source if it available and
 *     invariant.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeTsc(void) {
    /* Don't bother with TSC if it's not marked as invariant (essentially anything newer than the
     * Intel Core 2 series should support it, but still, it's good to check). */
    uint64_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (!(Edx & bit_TSC)) {
        return;
    }

    __cpuid(0x80000000, Eax, Ebx, Ecx, Edx);
    if (Eax < 0x80000007) {
        return;
    }

    __cpuid(0x80000007, Eax, Ebx, Ecx, Edx);
    if (!(Edx & 0x100)) {
        return;
    }

    /* By now we're sure we have invariant TSC; There is a way to extract the exact TSC frequency
     * (via leafs 15h and 16h) but that doesn't seem too supported (we probably should also
     * implement this path though), so, what we'll do instead, is calibrate it against the HPET. */
    uint64_t Ticks = (__uint128_t)(10 * EV_MILLISECS) * HalpGetHpetFrequency() / EV_SECS;
    for (int i = 0; i < 5; i++) {
        uint64_t StartTsc = __rdtsc();
        uint64_t StartHpet = HalpGetHpetTicks();

        while (HalpGetHpetTicks() < StartHpet + Ticks)
            ;

        uint64_t EndTsc = __rdtsc();
        uint64_t EndHpet = HalpGetHpetTicks();

        /* We're going to guess that the fastest run is the correct one; Maybe we should change this
         * to avg the values instead? Not 100% sure. */
        uint64_t DeltaTsc = EndTsc - StartTsc;
        uint64_t DeltaHpet = EndHpet - StartHpet;
        uint64_t CurrentFrequency = (__uint128_t)DeltaTsc * HalpGetHpetFrequency() / DeltaHpet;
        if (CurrentFrequency > Frequency) {
            Frequency = CurrentFrequency;
        }
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns the frequency (in Hz) of the TSC.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Frequency of the timer.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalpGetTscFrequency(void) {
    return Frequency;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function returns how many timer ticks have elapsed since the TSC was initialized.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     How many ticks have elapsed since system boot. Multiply it by the timer period to get how
 *     many nanoseconds have elapsed.
 *-----------------------------------------------------------------------------------------------*/
uint64_t HalpGetTscTicks(void) {
    return __rdtsc();
}
