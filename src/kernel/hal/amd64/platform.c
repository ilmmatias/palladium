/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <amd64/msr.h>
#include <vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines.
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *     IsBsp - Set this to 1 if we're the bootstrap processor, or 0 otherwise.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializePlatform(KeProcessor *Processor, int IsBsp) {
    WriteMsr(0xC0000102, (uint64_t)Processor);
    HalpSetIrql(KE_IRQL_PASSIVE);
    HalpInitializeGdt(Processor);
    HalpInitializeIdt(Processor);

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

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals the CPU we're in a busy loop (waiting for a spin lock probably), and
 *     we are safe to drop the CPU state into power saving.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpPauseProcessor(void) {
    __asm__ volatile("pause");
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function signals the CPU we're either fully done with any work this CPU will ever do
 *     (aka, panic), or that we're waiting some interrupt/external event.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpStopProcessor(void) {
    __asm__ volatile("hlt");
}
