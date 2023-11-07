/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>

static BootLoaderImage *LoadedImages;
static uint32_t LoadedImageCount;

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function saves up all images the boot loader prepared for us.
 *     We need to do this before allocating any memory.
 *
 * PARAMETERS:
 *     BootData - Data prepared by the boot loader for us.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiSaveBootStartDrivers(LoaderBootData *BootData) {
    LoadedImages = BootData->Images.Entries;
    LoadedImageCount = BootData->Images.Count;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function runs all the boot start driver entry points.
 *     After this, we should be ready to read more drivers from the disk.
 *
 * PARAMETERS:
 *     None.
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void KiRunBootStartDrivers(void) {
    /* Skipping the first image, as it is certain to be the kernel itself. */
    for (uint32_t i = 1; i < LoadedImageCount; i++) {
        ((void (*)(void))LoadedImages[i].EntryPoint)();
    }
}
