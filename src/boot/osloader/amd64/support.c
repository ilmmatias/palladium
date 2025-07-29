/* SPDX-FileCopyrightText: (C) 2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <cpuid.h>
#include <crt_impl/rand.h>
#include <platform.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function attempts to initialize the random number generator using the
 *     architecture-specific source.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if we initialized it, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool OslpInitializeArchEntropy(void) {
    /* RDSEED is seemingly a non determistic RNG, and we can use that to seed the PRNG on any new
       enough computer (it's VERY slow, so we should only use it as seed).
       RDRAND is a bit more supported, but gives no direct access to the hardware RNG.
       TSC/cycle counter is the last option, and should be supported on pretty much everything. */

    uint32_t SeedLow = 0, SeedHigh = 0;
    uint32_t Eax, Ebx, Ecx, Edx;

    do {
        __get_cpuid_count(7, 0, &Eax, &Ebx, &Ecx, &Edx);
        if (Ebx & bit_RDSEED) {
            __asm__ volatile("rdseed %0" : "=r"(SeedLow));
            __asm__ volatile("rdseed %0" : "=r"(SeedHigh));
            break;
        }

        __cpuid(1, Eax, Ebx, Ecx, Edx);
        if (Ecx & bit_RDRND) {
            __asm__ volatile("rdrand %0" : "=r"(SeedLow));
            __asm__ volatile("rdrand %0" : "=r"(SeedHigh));
        } else if (Edx & bit_TSC) {
            __asm__ volatile("rdtsc" : "=a"(SeedLow), "=d"(SeedHigh));
        }
    } while (false);

    if (SeedLow || SeedHigh) {
        __srand64(((uint64_t)SeedHigh << 32) | SeedLow);
        return true;
    } else {
        return false;
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function checks if the host machine is compatible with the operating system.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     true if we are compatible, false otherwise.
 *-----------------------------------------------------------------------------------------------*/
bool OslpCheckArchSupport(void) {
    /* x86-64 machines are guaranteed to have a base level of support (so, we don't need to check
     * for some things if we reached this point). We probably don't need to check for RDTSC nor
     * APIC support either (but we still do, at least for now). */

    uint32_t Eax, Ebx, Ecx, Edx;
    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (!(Ecx & bit_CMPXCHG16B)) {
        OslPrint("Your processor does not support the CMPXCHG16B instruction.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    } else if (!(Edx & bit_TSC)) {
        OslPrint("Your processor does not support the RDTSC instruction.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    } else if (!(Edx & bit_APIC)) {
        OslPrint("Your processor does not support APIC operation.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return false;
    }

    return true;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initializes the arch-specific boot block fields.
 *
 * PARAMETERS:
 *     BootBlock - Which structure to initialize.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslpInitializeArchBootData(OslpBootBlock* BootBlock) {
    /* This is our initial (rough) estimate, essentially used only for small delays during the boot
     * process. It should get superseded by either the HPET, another timer, or a more properly
     * calibrated TSC frequency (if we have an invariant TSC) when the kernel gets further in its
     * initialization process. */
    uint64_t StartTsc = __rdtsc();
    gBS->Stall(5000);
    uint64_t EndTsc = __rdtsc();
    BootBlock->Arch.CycleCounterFrequency = (EndTsc - StartTsc) * (1000000 / 5000);
}
