/* SPDX-FileCopyrightText: (C) 2023-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <kernel/halp.h>
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

    /* We only support clang for now, but maybe we should add GCC support + extend this code to
     * handle it in the future? */
    VidPrint(
        VID_MESSAGE_INFO,
        "Kernel",
        "built on %s using Clang %d.%d.%d\n",
        __DATE__,
        __clang_major__,
        __clang_minor__,
        __clang_patchlevel__);

    /* Stage 1 (BSP): Memory manager initialization; This won't mark the OSLOADER pages as free
     * just yet. */
    MiInitializeEarlyPageAllocator(LoaderBlock);
    MiInitializePool();
    MiInitializePageAllocator();

    /* Stage 2 (BSP): Save all the remaining data that we care about from the loader block. */
    KiSaveAcpiData(LoaderBlock);
    KiSaveBootStartDrivers(LoaderBlock);

    /* Stage 3 (BSP): Release and unmap all OSLOADER memory regions; Everything we need should have
     * already been copied to kernel land, so this should be safe. */
    MiReleaseBootRegions();

    /* Stage 4 (BSP): Early platform/arch initialization. */
    HalpInitializeBootProcessor();
    if (HalpOnlineProcessorCount == 1) {
        VidPrint(VID_MESSAGE_INFO, "Kernel", "1 processor online\n");
    } else {
        VidPrint(VID_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpOnlineProcessorCount);
    }

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
    /* Stage 6 (BSP): Initialize all boot drivers; We can't load anything further than this without
       them. */
    KiRunBootStartDrivers();

    /* Dump the PFN stats; Some of the page stat categories can/will intersect with the used/free
     * pages count, so beware of that. */
    VidPrintSimple("\ndumping the PFN stats:\n");
    VidPrintSimple("       total pages: %llu\n", MiTotalSystemPages);
    VidPrintSimple("    reserved pages: %llu\n", MiTotalReservedPages);
    VidPrintSimple("      cached pages: %llu\n", MiTotalCachedPages);
    VidPrintSimple("        used pages: %llu\n", MiTotalUsedPages);
    VidPrintSimple("        free pages: %llu\n", MiTotalFreePages);
    VidPrintSimple("        boot pages: %llu\n", MiTotalBootPages);
    VidPrintSimple("    graphics pages: %llu\n", MiTotalGraphicsPages);
    VidPrintSimple("         pte pages: %llu\n", MiTotalPtePages);
    VidPrintSimple("         pfn pages: %llu\n", MiTotalPfnPages);
    VidPrintSimple("        pool pages: %llu\n", MiTotalPoolPages);

    /* Dump the per-tag pool stats. */
    VidPrintSimple("\ndumping the tag stats:\n");
    VidPrintSimple(
        "tag  | allocs           | bytes            | max allocs       | max bytes       \n");
    for (int i = 0; i < 256; i++) {
        for (RtSList *ListHeader = MiPoolTagListHead[i].Next; ListHeader;
             ListHeader = ListHeader->Next) {
            MiPoolTrackerHeader *Header =
                CONTAINING_RECORD(ListHeader, MiPoolTrackerHeader, ListHeader);
            VidPrintSimple(
                "%.4s | %-16llu | %-16llu | %-16llu | %-16llu\n",
                Header->Tag,
                Header->Allocations,
                Header->AllocatedBytes,
                Header->MaxAllocations,
                Header->MaxAllocatedBytes);
        }
    }

    while (true) {
        StopProcessor();
    }
}
