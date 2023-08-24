/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <crt_impl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86/bios.h>
#include <x86/cpuid.h>

#define BASE_MESSAGE                                                          \
    "An error occoured while trying to load the selected operating system.\n" \
    "Your device does not support one or more of the required features "

static MemoryArena KernelRegion;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function sets up any architecture-dependent features, and gets the system ready for
 *     going further in the boot process.
 *
 * PARAMETERS:
 *     BootBlock - Information obtained while setting up the boot environment.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmInitArch(void *BootBlock) {
    BiosBootBlock *Data = (BiosBootBlock *)BootBlock;
    BiosDetectDisks(Data);

    /* We're not sure if rdseed (true RNG) or rdrand is available; Seed the PRNG using the clock
       cycle counter. */
    uint64_t Seed;
    __asm__ volatile("rdtsc" : "=A"(Seed));
    __srand64(Seed);

    /* Seed virtual region allocator with a single region, containing all the high/kernel
       space. */
    KernelRegion.Base = ARENA_BASE;
    KernelRegion.Size = ARENA_SIZE;
    KernelRegion.Next = NULL;
    BmMemoryArena = &KernelRegion;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function validates if the host computer is capable of running the Palladium kernel.
 *
 * PARAMETERS:
 *     None
 *
 * RETURN VALUE:
 *     None; Does not return if the host is incompatible.
 *-----------------------------------------------------------------------------------------------*/
void BmCheckCompatibility(void) {
    uint32_t Eax, Ebx, Ecx, Edx;

    /* Palladium targets at least an Intel Haswell (4th gen) processor, or an AMD Zen (1st gen)
       processor. As such, we can assume the host should have at least SSE4.2, AVX2, BMI2, LM,
       XSAVE.
       If all those features are available, we're assuming this is a support processor. */

    __cpuid(1, Eax, Ebx, Ecx, Edx);
    if (!(Ecx & bit_SSE42)) {
        BmPanic(BASE_MESSAGE "(SSE42).\n");
    } else if (!(Ecx & bit_XSAVE)) {
        BmPanic(BASE_MESSAGE "(XSAVE).\n");
    }

    __get_cpuid_count(7, 0, &Eax, &Ebx, &Ecx, &Edx);
    if (!(Ebx & bit_AVX2)) {
        BmPanic(BASE_MESSAGE "(AVX2).\n");
    } else if (!(Ebx & bit_BMI2)) {
        BmPanic(BASE_MESSAGE "(BMI2).\n");
    }

    __cpuid(0x80000001, Eax, Ebx, Ecx, Edx);
    if (!(Edx & bit_LM)) {
        BmPanic(BASE_MESSAGE "(LM).\n");
    } else if (!(Edx & bit_PDPE1GB)) {
        BmPanic(BASE_MESSAGE "(PDPE1GB).\n");
    }
}
