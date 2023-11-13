/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <halp.h>
#include <ki.h>
#include <mi.h>
#include <psp.h>
#include <vidp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point.
 *     The next phase of boot is done after the process manager creates the system thread.
 *
 * PARAMETERS:
 *     IsBsp - Set this to 1 if we're the bootstrap processor, or 0 otherwise.
 *     Processor - Pointer to the processor-specific structure.
 *     LoaderData - Where bootmgr placed our boot data; Only valid in the BSP.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiSystemStartup(int IsBsp, void *Processor, LoaderBootData *BootData) {
    if (IsBsp) {
        /* Stage 0: Early output initialization; We need this, or we can't show error messages. */
        VidpInitialize(BootData);

        /* Stage 1: Memory manager initialization. */
        MiInitializePageAllocator(BootData);
        MiInitializePool(BootData);

        /* Stage 2.1: Save all remaining info from the boot loader. */
        KiSaveAcpiData(BootData);
        KiSaveBootStartDrivers(BootData);
    }

    /* Stage 2.2: Early platform/arch initialization. */
    HalpInitializePlatform(IsBsp, Processor);

    if (IsBsp) {
        /* Stage 2.3: APs should be up, show how many we have. */
        VidPrint(VID_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpProcessorCount);

        /* Stage 3.1: Create the initial system thread. */
        PspCreateSystemThread();
    }

    /* Stage 3.2: Create the idle thread, and spin up the scheduler. */
    PspCreateIdleThread();
    PspInitializeScheduler(IsBsp);

    while (1)
        ;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the post-scheduler entry point; We're responsible for finishing the boot
 *     process.
 *
 * PARAMETERS:
 *     LoaderData - Where bootmgr placed our boot data.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiContinueSystemStartup(void *) {
    /* Stage 4: Initialize all boot drivers; We can't load anything further than this without
       them. */
    KiRunBootStartDrivers();

    while (1)
        ;
}
