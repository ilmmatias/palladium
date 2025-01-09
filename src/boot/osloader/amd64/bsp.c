/* SPDX-FileCopyrightText: (C) 2024-2025 ilmmatias
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <amd64/processor.h>
#include <console.h>
#include <memory.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function creates the boot processor structure (which also contains the initial kernel
 *     stack).
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     Pointer to the KeProcessor structure, or NULL on failure.
 *-----------------------------------------------------------------------------------------------*/
void *OslpInitializeBsp(void) {
    KeProcessor *BootProcessor = OslAllocatePages(sizeof(KeProcessor), PAGE_BOOT_PROCESSOR);
    if (!BootProcessor) {
        OslPrint("The system ran out of memory while creating the boot processor structure.\r\n");
        OslPrint("The boot process cannot continue.\r\n");
        return NULL;
    }

    /* HalpInitializePlatform+the other HAL functions should setup the BSP (and any other AP)
     * structures, so that's all we need to do. */
    return BootProcessor;
}
