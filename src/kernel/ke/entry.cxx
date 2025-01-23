/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <ki.h>
#include <mi.h>
#include <psp.h>
#include <stdio.h>
#include <vidp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point for the boot processor (we
 *     get here directly from osloader).
 *
 * PARAMETERS:
 *     LoaderBlockPage - Physical address of the OSLOADER's boot block.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
extern "C" [[noreturn]] void KiBspEntry(uint64_t LoaderBlockPage) {
    KiLoaderBlock *LoaderBlock =
        (KiLoaderBlock *)MiEnsureEarlySpace(LoaderBlockPage, sizeof(KiLoaderBlock));

    /* Stage 1 (BSP): Display initialization (for early debugging). */
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

    /* Stage 1 (BSP): Memory manager initialization; This won't mark the OSLOADER pages as free
     * just yet. */
    MiInitializePool(LoaderBlock);
    MiInitializePageAllocator(LoaderBlock);

    /* Stage 2 (BSP): Save all the remaining data that we care about from the loader block. */
    KiSaveAcpiData(LoaderBlock);
    KiSaveBootStartDrivers(LoaderBlock);

    /* Stage 3 (BSP): KeProcessor structure initialization; This will get us out of the boot stack,
     * after which all osloader memory map entries are free for us to use.
     * This function does not return (it jumps into KiInitializeBspScheduler). */
    HalpInitializeBsp();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the pre-scheduler BSP initialization function; HalpInitializeBsp puts us
 *     here after changing the stack, and our just is pretty much just to get the scheduler going.
 *
 * PARAMETERS:
 *     LoaderBlockPage - Physical address of the OSLOADER's boot block.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
extern "C" [[noreturn]] void KiInitializeBspScheduler(void) {
    VidPrint(VID_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpProcessorCount);

    /* Stage 4 (BSP): Release and unmap all OSLOADER memory regions; Everything we need should have
     * already been copied to kernel land, so this should be safe. */
    MiReleaseBootRegions();

    /* Stage 5 (BSP): Create the initial system and idle threads, and spin up the scheduler. */
    PspCreateSystemThread();
    PspCreateIdleThread();
    PspInitializeScheduler(1);

    while (1) {
        HalpStopProcessor();
    }
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point for the
 *     application/secondary processors.
 *
 * PARAMETERS:
 *     Processor - Virtual address of this processor's data block.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
extern "C" [[noreturn]] void KiApEntry(KeProcessor *Processor) {
    /* Stage 0 (AP): Early platform/arch initialization. */
    HalpInitializeAp(Processor);

    /* Stage 1 (AP): Create the idle thread, and spin up the scheduler (we'll be on the wait for
     * threads to run). */
    PspCreateIdleThread();
    PspInitializeScheduler(0);

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
    /* Stage 6 (BSP): Initialize all boot drivers; We can't load anything further than this without
       them. */
    KiRunBootStartDrivers();

    while (1) {
        HalpStopProcessor();
    }
}
