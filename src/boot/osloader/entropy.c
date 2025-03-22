/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <console.h>
#include <crt_impl/rand.h>
#include <efi/spec.h>

#ifdef ARCH_amd64
#include <cpuid.h>
#endif

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function initialize the entropy/random number generator source for the memory arena
 *     allocator.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void OslpInitializeEntropy(void) {
#ifdef ARCH_amd64
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
        return;
    }
#endif

    /* Fallback, try using the UEFI entropy source; If we can't do that, the seed will stay as
     * 0. */
    do {
        EFI_RNG_PROTOCOL *Rng = NULL;
        if (gBS->LocateProtocol(&gEfiRngProtocolGuid, NULL, (VOID **)Rng) != EFI_SUCCESS) {
            break;
        }

        uint64_t Seed = 0;
        if (Rng->GetRNG(Rng, NULL, 8, (UINT8 *)&Seed) != EFI_SUCCESS) {
            break;
        }

        __srand64(Seed);
        return;
    } while (false);

    OslPrint("Failed to initialize the entropy source.\r\n");
    OslPrint("KASLR will be predictable across reboots.\r\n");
}
