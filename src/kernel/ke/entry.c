/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
#include <kernel/kdp.h>
#include <kernel/ki.h>
#include <kernel/mi.h>
#include <kernel/psp.h>
#include <kernel/vidp.h>

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
    /* Hello, World! We're essentially still fresh out of the loader land, so, take over the boot
     * framebuffer, and get us to a basic state (where the kernel/HAL is managing the basic
     * resources like exception/interrupt handling). */
    VidpInitialize(LoaderBlock);
    MiInitializeEarlyPageAllocator(LoaderBlock);
    HalpInitializePlatform(LoaderBlock);

    /* If requested, initialize the debugger as early as possible (should be doable now that the
     * early HAL stuff is up). */
    KdpInitializeDebugger(LoaderBlock);

    /* Announce we're officially online (the HAL probably already printed some stuff, but this is
     * the first point where attached debuggers will receive messages as well). */
#ifdef NDEBUG
    KdPrint(
        KD_TYPE_INFO,
        "palladium kernel for %s, git commit %s, release build\n",
        KE_ARCH,
        KE_GIT_HASH);
#else
    KdPrint(
        KD_TYPE_INFO,
        "palladium kernel for %s, git commit %s, debug build\n",
        KE_ARCH,
        KE_GIT_HASH);
#endif

    /* We only support clang for now, but maybe we should add GCC support + extend this code to
     * handle it in the future? */
    KdPrint(
        KD_TYPE_INFO,
        "built on %s using Clang %d.%d.%d\n",
        __DATE__,
        __clang_major__,
        __clang_minor__,
        __clang_patchlevel__);

    /* Get the memory manager fully online (MmAllocate* functions are available after this). */
    MiInitializePool();
    MiInitializePageAllocator();
    KdPrint(
        KD_TYPE_INFO, "managing %llu MiB of memory\n", MiTotalSystemPages * MM_PAGE_SIZE / 1048576);

    /* The loader data will become inaccessible once we release/unmap all the remaining OSLOADER
     * regions, so save the required remaining data. After this, the stack trace on KeFatalError
     * will start working properly (as it depends on the module data to unwind). */
    KiSaveBootStartDrivers(LoaderBlock);
    MiReleaseBootRegions();

    /* It should now be safe to wrap up the HAL initialization (which will also bring up the
     * secondary processors). */
    HalpInitializeBootProcessor();
    if (HalpOnlineProcessorCount == 1) {
        KdPrint(KD_TYPE_INFO, "1 processor online\n");
    } else {
        KdPrint(KD_TYPE_INFO, "%u processors online\n", HalpOnlineProcessorCount);
    }

    /* At last, get the scheduler up so that we can get out of the system/boot stack, and into the
     * initial system thread. */
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
    /* Application processors are a bit boring; The BSP already initialized everything, so we just
     * need to get our HAL stuff up, and the idle thread. */
    HalpInitializeApplicationProcessor(Processor);
    PspCreateIdleThread();
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point for all processors.
 *     The boot processor gets here from osloader (or from the stack change reentry), while APs get
 *     here from the SMP initialization code.
 *
 * PARAMETERS:
 *     LoaderBlock - Data prepared by the boot loader for us.
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
    /* Get all of the required boot modules up; This should let us load the remaining drivers from
     * the disk. */
    KiRunBootStartDrivers();

    while (true) {
        StopProcessor();
    }
}
