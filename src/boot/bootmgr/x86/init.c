/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <x86/bios.h>

/* We know the internals of our CRT, we can use that on our favor to initialize the whole 64-bits
   of the internal state. */
extern uint64_t __rand_state;

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

    __asm__ volatile("rdtsc" : "=A"(__rand_state));
    if (!__rand_state) {
        __rand_state = 1;
    }
}
