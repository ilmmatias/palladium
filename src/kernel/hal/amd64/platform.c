/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <amd64/msr.h>
#include <mm.h>
#include <vid.h>

extern void KiInitializeBspScheduler(void);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function allocates the BSP processor structure (along with its stack), and runs any
 *     early arch-specific initialization routines required for the boot processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void HalpInitializeBsp(void) {
    KeProcessor *Processor = MmAllocatePool(sizeof(KeProcessor), "Halp");
    if (!Processor) {
        KeFatalError(KE_PANIC_INSTALL_MORE_MEMORY);
    }

    WriteMsr(0xC0000102, (uint64_t)Processor);
    HalpSetIrql(KE_IRQL_PASSIVE);
    HalpInitializeGdt(Processor);
    HalpInitializeIdt(Processor);
    HalpInitializeIoapic();
    HalpInitializeApic();
    HalpEnableApic();
    Processor->ApicId = HalpReadLapicId();
    HalpInitializeHpet();
    HalpInitializeSmp();
    HalpInitializeApicTimer();

    __asm__ volatile("mov %0, %%rax\n"
                     "mov %1, %%rsp\n"
                     "jmp *%%rax\n"
                     :
                     : "r"(KiInitializeBspScheduler),
                       "r"(Processor->SystemStack + sizeof(Processor->SystemStack))
                     : "%rax");

    while (1) {
        HalpStopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines required for the
 *     secondary/application processors.
 *
 * PARAMETERS:
 *     Processor - Pointer to the processor-specific structure.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeAp(KeProcessor *Processor) {
    WriteMsr(0xC0000102, (uint64_t)Processor);
    HalpSetIrql(KE_IRQL_PASSIVE);
    HalpInitializeGdt(Processor);
    HalpInitializeIdt(Processor);
    HalpEnableApic();
    Processor->ApicId = HalpReadLapicId();
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
