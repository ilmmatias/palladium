/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <halp.h>
#include <ki.h>
#include <mi.h>
#include <rt.h>
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
[[noreturn]] void KiSystemStartup(int IsBsp, void *Processor, void *LoaderData) {
    if (IsBsp) {
        /* Stage 0: Early output initialization; We need this, or we can't show error messages. */
        VidpInitialize(LoaderData);

        /* Stage 1: Memory manager initialization. */
        MiInitializePageAllocator(LoaderData);
        MiInitializeVirtualMemory(LoaderData);

        /* Stage 2.1: Save all remaining info from the boot loader. */
        HalpSaveAcpiData(LoaderData);
    }

    /* Stage 2.2: Early platform/arch initialization. */
    HalpInitializePlatform(IsBsp, Processor);

    if (IsBsp) {
        /* Stage 2.3: APs should be up, show how many we have. */
        VidPrint(KE_MESSAGE_INFO, "Kernel", "%u processors online\n", HalpProcessorCount);

        /* Stage 3: Spin up all boot-time drivers; Can't load anything else without them. */
        KiRunBootStartDrivers(LoaderData);
    }

    while (1)
        ;
}
