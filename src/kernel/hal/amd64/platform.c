/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/halp.h>
#include <amd64/msr.h>
#include <mm.h>

[[noreturn]] extern void KiSystemStartup(KiLoaderBlock *LoaderBlock, KeProcessor *Processor);

/* This is pretty important to be here, as we need it before the memory manager initialization
 * to switch out of the bootloader stack. */
static KeProcessor BootProcessor __attribute__((aligned(4096))) = {};

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function re-enters KiSystemStartup using the boot processor kernel stack instead of the
 *     osloader temporary stack.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeBootStack(KiLoaderBlock *LoaderBlock) {
    __asm__ volatile("mov %0, %%rax\n"
                     "mov %1, %%rcx\n"
                     "mov %2, %%rdx\n"
                     "mov %3, %%rsp\n"
                     "jmp *%%rax\n"
                     :
                     : "r"(KiSystemStartup),
                       "r"(LoaderBlock),
                       "r"(&BootProcessor),
                       "r"(BootProcessor.SystemStack + sizeof(BootProcessor.SystemStack) - 8)
                     : "%rax", "%rcx", "%rdx");

    while (true) {
        HalpStopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs any early arch-specific initialization routines required for the boot
 *     processor.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void HalpInitializeBootProcessor(void) {
    /* This should always go well (if it doesn't, KeFatalError is called outside of us). */
    WriteMsr(0xC0000102, (uint64_t)&BootProcessor);
    HalpInitializeGdt(&BootProcessor);
    HalpInitializeIdt(&BootProcessor);
    HalpInitializeIoapic();
    HalpInitializeApic();
    HalpEnableApic();
    BootProcessor.ApicId = HalpReadLapicId();
    HalpInitializeHpet();
    HalpInitializeSmp();
    HalpInitializeApicTimer();
    HalpSetIrql(KE_IRQL_PASSIVE);
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
void HalpInitializeApplicationProcessor(KeProcessor *Processor) {
    WriteMsr(0xC0000102, (uint64_t)Processor);
    HalpInitializeGdt(Processor);
    HalpInitializeIdt(Processor);
    HalpEnableApic();
    Processor->ApicId = HalpReadLapicId();
    HalpInitializeApicTimer();
    HalpSetIrql(KE_IRQL_PASSIVE);
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
