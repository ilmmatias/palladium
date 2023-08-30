/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <ke.h>
#include <mm.h>
#include <vid.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the kernel's architecture-independent entry point.
 *     We're responsible for the "stage 0" of the system boot (initializing the memory manager,
 *     IO manager, boot-start drivers, and the process manager).
 *     The next phase of boot is done after the process manager creates the system thread.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void KiSystemStartup(void *LoaderData) {
    VidpInitialize();
    MiPreparePageAllocator(LoaderData);
    KiSaveAcpiData(LoaderData);
    KiRunBootStartDrivers(LoaderData);
    KeFatalError(0);
}
