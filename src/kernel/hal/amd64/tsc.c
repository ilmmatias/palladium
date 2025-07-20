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
    uint64_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (!(Edx & bit_TSC)) {
        VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "RDTSC is unavailable\n");
        return;
    }

    __cpuid(0x80000000, Eax, Ebx, Ecx, Edx);
    if (Eax < 0x80000007) {
        VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "invariant TSC is unavailable\n");
        return;
    }

    __cpuid(0x80000007, Eax, Ebx, Ecx, Edx);
    if (!(Edx & 0x100)) {
        VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "invariant TSC is unavailable\n");
        return;
    }

    /* By now we're sure we have invariant TSC; At first, let's attempt to extract the exact TSC
     * frequency via CPUID leafs 15h and 16h (which is really only supported on newer Intel
     * processors, so not really that widely supported). */
    do {
        __cpuid(0, Eax, Ebx, Ecx, Edx);
        if (Eax < 0x15) {
            VidPrint(VID_MESSAGE_DEBUG, "Kernel HAL", "CPUID leaf 15h is unavailable\n");
            break;
        }

        /* Leaf 15h is pretty unreliable, as most processors don't set CoreFreq; If at least we have
         * leaf 16h, we can use that to get the core crystal freq. */
        uint64_t RatioDenom, RatioNum, CoreFreq;
        __cpuid(0x15, RatioDenom, RatioNum, CoreFreq, Edx);
        if (!CoreFreq && Eax >= 0x16) {
            __cpuid(0x16, Eax, Ebx, Ecx, Edx);
            CoreFreq = (Eax * 10000000) * (RatioDenom / RatioNum);
        }

        /* If even that was not good, then just bail out. */
        if (!CoreFreq) {
            VidPrint(
                VID_MESSAGE_DEBUG,
                "Kernel HAL",
                "CPUID leaf 15h has no core crystal clock frequency\n");
            break;
        }

        Frequency = CoreFreq * (RatioNum / RatioDenom);
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

            /* We're going to guess that the fastest run is the correct one; Maybe we should change
             * this to avg the values instead? Not 100% sure. */
            uint64_t DeltaTsc = EndTsc - StartTsc;
            uint64_t DeltaHpet = EndHpet - StartHpet;
            uint64_t CurrentFrequency = (__uint128_t)DeltaTsc * HalpGetHpetFrequency() / DeltaHpet;
            if (CurrentFrequency > Frequency) {
                Frequency = CurrentFrequency;
            }
        }
    }

    /* Clean up the TSC to start in a clean slate. */
    WriteMsr(HALP_TSC_MSR, 0);

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
