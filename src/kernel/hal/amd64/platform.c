/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/intrin.h>
#include <kernel/ke.h>

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
    /* The BSP gets its KeGetCurrentProcessor() initialized here instead of
     * HalpInitializeXProcessor (because we need it for early KeFatalError).
     * Maybe we should create a function specifically for this? */
    WriteMsr(0xC0000102, (uint64_t)&BootProcessor);
    BootProcessor.StackBase = BootProcessor.SystemStack;
    BootProcessor.StackLimit = BootProcessor.SystemStack + sizeof(BootProcessor.SystemStack);

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
        StopProcessor();
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
    HalpInitializeGdt(&BootProcessor);
    HalpInitializeIdt(&BootProcessor);
    HalpInitializeIoapic();
    HalpInitializeApic();
    HalpEnableApic();
    BootProcessor.ApicId = HalpReadLapicId();
    HalpInitializeHpet();
    HalpInitializeSmp();
    HalpInitializeApicTimer();
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
    Processor->StackBase = Processor->SystemStack;
    Processor->StackLimit = Processor->SystemStack + sizeof(Processor->SystemStack);
    HalpInitializeGdt(Processor);
    HalpInitializeIdt(Processor);
    HalpEnableApic();
    Processor->ApicId = HalpReadLapicId();
    HalpInitializeApicTimer();
}
