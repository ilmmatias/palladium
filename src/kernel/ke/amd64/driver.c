/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <amd64/boot.h>

static LoaderImage *LoadedImages;
static uint32_t LoadedImageCount;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves the loaded images pointer (which should be already in kernel memory),
 *     and runs all the boot start driver entry points.
 *     WARNING: After running this function, LoaderData is probably trashed, as the drivers
 *     might call MmAllocatePool() or MmAllocatePages(), and overwrite the boot region where
 *     LoaderData was saved. Make sure to save/use everything required for the boot process before
 *     calling this!
 *
 * PARAMETERS:
 *     LoaderData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiRunBootStartDrivers(void *LoaderData) {
    LoaderBootData *BootData = LoaderData;

    LoadedImages = BootData->Images.Entries;
    LoadedImageCount = BootData->Images.Count;

    /* Skipping the first image, as it is certain to be the kernel itself. */
    for (uint32_t i = 1; i < LoadedImageCount; i++) {
        ((void (*)(void))LoadedImages[i].EntryPoint)();
    }
}
