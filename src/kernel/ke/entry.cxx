/* SPDX-FileCopyrightText: (C) 2023-2024 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

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
 *     LoaderBlockPage - Physical address of the OSLOADER's boot block.
 *     ProcessorPage - Either the physical address (for the boot processor) or the virtual
 *                     address (for the APs) of this processor's block.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
extern "C" [[noreturn]] void KiSystemStartup(uint64_t LoaderBlockPage, uint64_t ProcessorPage) {
    KiLoaderBlock *LoaderBlock;
    KeProcessor *Processor;

    if (LoaderBlockPage) {
        LoaderBlock = (KiLoaderBlock *)MiEnsureEarlySpace(LoaderBlockPage, sizeof(KiLoaderBlock));
        Processor = (KeProcessor *)MiEnsureEarlySpace(ProcessorPage, sizeof(KeProcessor));
    } else {
        Processor = (KeProcessor *)ProcessorPage;
    }

    if (LoaderBlockPage) {
        /* Stage 1: Memory manager initialization; This won't mark the OSLOADER pages as free
         * just yet. */
        MiInitializePool(LoaderBlock);
        MiInitializePageAllocator(LoaderBlock);

        /* Stage 2: Display initialization (for early debugging). */
        VidpInitialize(LoaderBlock);

#ifdef NDEBUG
        VidPrint(
            VID_MESSAGE_INFO,
            "Kernel",
            "palladium kernel for %s, git commit %s, release build\n",
            KE_ARCH,
            KE_GIT_HASH);
#else
        VidPrint(
            VID_MESSAGE_INFO,
            "Kernel",
            "palladium kernel for %s, git commit %s, debug build\n",
            KE_ARCH,
            KE_GIT_HASH);
#endif

        /* Stage 3.1: Save all remaining info from the boot loader. */
        KiSaveAcpiData(LoaderBlock);
        KiSaveBootStartDrivers(LoaderBlock);
    }

    /* Stage 3.2: Early platform/arch initialization. */
    if (LoaderBlockPage) {
        HalpInitializePlatform(Processor, 1);
        VidPrint(VID_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpProcessorCount);
    } else {
        HalpInitializePlatform(Processor, 0);
    }

    /* Stage 4: Create the initial system thread, idle thread, and spin up the scheduler. */
    PspCreateSystemThread();
    PspCreateIdleThread();
    PspInitializeScheduler(LoaderBlockPage != 0);

    while (1) {
        HalpStopProcessor();
    }
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
extern "C" [[noreturn]] void KiContinueSystemStartup(void *) {
    /* Stage 5: Initialize all boot drivers; We can't load anything further than this without
       them. */
    KiRunBootStartDrivers();

    while (1) {
        HalpStopProcessor();
    }
}
