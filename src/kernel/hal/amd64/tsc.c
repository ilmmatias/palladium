/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <cpuid.h>
#include <kernel/ev.h>
#include <kernel/halp.h>
#include <kernel/intrin.h>
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
    if (!(HalpPlatformFeatures & HALP_FEATURE_TSC)) {
        VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "RDTSC is unavailable\n");
        return;
    } else if (!(HalpPlatformFeatures & HALP_FEATURE_INVARIANT_TSC)) {
        VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "invariant TSC is unavailable\n");
        return;
    }

    /* By now we're sure we have invariant TSC; At first, let's attempt to extract the exact TSC
     * frequency via CPUID leafs 15h and 16h (which is really only supported on newer Intel
     * processors, so not really that widely supported). */
    do {
        if (HalpPlatformMaxLeaf < HALP_CPUID_TSC_FREQUENCY) {
            VidPrint(
                VID_MESSAGE_DEBUG, "Kernel HAL", "cannot acquire the TSC frequency via CPUID\n");
            break;
        }

        uint64_t RatioDenom, RatioNum, CoreFreq, Unused;
        __cpuid(HALP_CPUID_TSC_FREQUENCY, RatioDenom, RatioNum, CoreFreq, Unused);
        if (!RatioNum) {
            VidPrint(
                VID_MESSAGE_DEBUG, "Kernel HAL", "cannot acquire the TSC frequency via CPUID\n");
            break;
        }

        /* Leaf 15h is pretty unreliable, as most processors don't set CoreFreq; If at least we have
         * leaf 16h, we can use that to get the core crystal freq. */
        if (!CoreFreq && HalpPlatformMaxLeaf >= HALP_CPUID_PROCESSOR_FREQUENCY) {
            uint64_t BaseFreq, Unused0, Unused1, Unused2;
            __cpuid(HALP_CPUID_PROCESSOR_FREQUENCY, BaseFreq, Unused0, Unused1, Unused2);
            CoreFreq = BaseFreq * 1000000 * RatioDenom / RatioNum;
        }

        /* If even that was not good, then just bail out. */
        if (!CoreFreq) {
            VidPrint(
                VID_MESSAGE_DEBUG, "Kernel HAL", "cannot acquire the TSC frequency via CPUID\n");
            break;
        }

        Frequency = CoreFreq * RatioNum / RatioDenom;
    } while (false);

    /* Otherwise, just calibrate it against the HPET. */
    if (!Frequency) {
        uint64_t Ticks = (__uint128_t)(10 * EV_MILLISECS) * HalpGetHpetFrequency() / EV_SECS;

        for (int i = 0; i < 5; i++) {
            uint64_t StartTsc = __rdtsc();
            uint64_t StartHpet = HalpGetHpetTicks();

            while (HalpGetHpetTicks() < StartHpet + Ticks)
                ;

            uint64_t EndTsc = __rdtsc();
            uint64_t EndHpet = HalpGetHpetTicks();

            uint64_t DeltaTsc = EndTsc - StartTsc;
            uint64_t DeltaHpet = EndHpet - StartHpet;
            Frequency += (__uint128_t)DeltaTsc * HalpGetHpetFrequency() / DeltaHpet;
        }

        Frequency /= 5;
    }

    /* Clean up the TSC to start in a clean slate. */
    WriteMsr(HALP_MSR_TSC, 0);

    VidPrint(
        VID_MESSAGE_DEBUG,
        "Kernel HAL",
        "found an invariant TSC (frequency = %llu.%02llu MHz)\n",
        Frequency / 1000000,
        (Frequency % 1000000) / 10000);
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
