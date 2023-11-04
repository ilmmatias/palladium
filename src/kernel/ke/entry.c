/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

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
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiSystemStartup(void *LoaderData) {
    /* Stage 0: Early output initialization; We need those, or we can't show error messages. */
    VidpInitialize(LoaderData);

    /* Stage 1: Memory manager initialization. */
    MiInitializePageAllocator(LoaderData);
    MiInitializeVirtualMemory(LoaderData);

    /* Stage 2: Early platform/arch initialization. */
    KiSaveAcpiData(LoaderData);
    KiInitializePlatform();

    /* Stage 2: Root drivers should be already loaded, wrap them up by calling their entry. */
    KiRunBootStartDrivers(LoaderData);

    while (1)
        ;
}
