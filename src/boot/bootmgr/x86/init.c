/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <crt_impl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86/bios.h>

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
