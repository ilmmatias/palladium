/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <halp.h>
#include <ki.h>
#include <mi.h>
#include <psp.h>
#include <vidp.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs the required BSP-only initialization routines.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
static void InitializeBootProcessor(KiLoaderBlock *LoaderBlock) {
    /* Stage 0 (BSP): Display initialization (for early debugging). */
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

    /* Stage 3 (BSP): Early platform/arch initialization. */
    HalpInitializeBootProcessor();
    VidPrint(VID_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpProcessorCount);

    /* Stage 4 (BSP): Release and unmap all OSLOADER memory regions; Everything we need should have
     * already been copied to kernel land, so this should be safe. */
    MiReleaseBootRegions();

    /* Stage 5 (BSP): Scheduler initialization. */
    PspCreateIdleThread();
    PspCreateSystemThread();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs the required AP-only initialization routines.
 *
 * PARAMETERS:
 *     Processor - Processor block pointer. This should be NULL if we need to trigger a stack change
 *     on the BSP.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
static void InitializeApplicationProcessor(KeProcessor *Processor) {
    /* Stage 0 (AP): Early platform/arch initialization. */
    HalpInitializeApplicationProcessor(Processor);

    /* Stage 1 (AP): Scheduler initialization. */
    PspCreateIdleThread();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point for all processors.
 *     The boot processor gets here from osloader (or from the stack change reentry), while APs get
 *     here from the SMP initialization code.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us. Should be a physical address if we
 *                   directly got here from osloader, a virtual address if we did a reentry, or NULL
 *                   if we got here from the AP startup process.
 *     Processor - Processor block pointer. This should be NULL if we need to trigger a stack change
 *                 on the BSP.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiSystemStartup(KiLoaderBlock *LoaderBlock, KeProcessor *Processor) {
    /* Trigger a stack change if we just arrived from the osloader; If your architecture doesn't
     * need this, then feel free to return from HalpInitializeBootStack, and the boot process will
     * continue as usual. */
    if (LoaderBlock && !Processor) {
        LoaderBlock = MiEnsureEarlySpace((uint64_t)LoaderBlock, sizeof(KiLoaderBlock));
        HalpInitializeBootStack(LoaderBlock);
    }

    /* Otherwise, spin up the basic processor initialization. */
    if (LoaderBlock) {
        InitializeBootProcessor(LoaderBlock);
    } else {
        InitializeApplicationProcessor(Processor);
    }

    /* Switch into the initial thread (this should finish up the scheduler initialization). */
    PspInitializeScheduler();
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
    /* Stage 6 (BSP): Initialize all boot drivers; We can't load anything further than this without
       them. */
    KiRunBootStartDrivers();

    while (true) {
        StopProcessor();
    }
}
