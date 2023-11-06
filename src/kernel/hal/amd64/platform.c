/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/halp.h>
#include <amd64/msr.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines.
 *
 * PARAMETERS:
 *     IsBsp - Set this to 1 if we're the bootstrap processor, or 0 otherwise.
 *     Processor - Pointer to the processor-specific structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializePlatform(int IsBsp, void *Processor) {
    WriteMsr(0xC0000102, (uint64_t)Processor);
    HalpInitializeGdt((HalpProcessor *)Processor);
    HalpInitializeIdt((HalpProcessor *)Processor);

    if (IsBsp) {
        HalpInitializeIoapic();
        HalpInitializeApic();
        HalpEnableApic();
        HalpInitializeHpet();
        HalpInitializeSmp();
    } else {
        HalpEnableApic();
    }

    HalpInitializeApicTimer();
}
