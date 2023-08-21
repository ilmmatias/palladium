/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <display.h>
#include <keyboard.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the specified PE file, validating the target architecture, and if
 *     it's an EXE (kernel), DLL, or SYS (driver) file.
 *     KASLR is enabled at all times, with the low bits of the virtual address being randomized.
 *
 * PARAMETERS:
 *     Path - Full path of the file.
 *     VirtualAddress - Output; Chosen virual address for the image.
 *
 * RETURN VALUE:
 *     Physical address of the image, or 0 if we failed to load it.
 *-----------------------------------------------------------------------------------------------*/
static uintptr_t LoadFile(const char *Path, uintptr_t *VirtualAddress) {
    FILE *Stream = fopen(Path, "rb");
    if (!Stream) {
        return 0;
    }

    (void)VirtualAddress;
    return 0;
}

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function loads up the system located in the given folder, jumping to execution
 *     afterwards.
 *
 * PARAMETERS:
 *     SystemFolder - Full path of the system folder (device()/path/to/System).
 *
 * RETURN VALUE:
 *     None.
 *-----------------------------------------------------------------------------------------------*/
void BmLoadPalladium(const char *SystemFolder) {
    BmSetColor(DISPLAY_COLOR_DEFAULT);
    BmInitDisplay();

    do {
        size_t KernelPathSize = strlen(SystemFolder) + 12;
        char *KernelPath = malloc(KernelPathSize);
        if (!KernelPath) {
            break;
        }

        snprintf(KernelPath, KernelPathSize, "%s/kernel.exe", SystemFolder);

        /* Delegate the loading job to the common PE loader. */
        uintptr_t KernelVirtualAddress;
        uintptr_t KernelPhysicalAddress = LoadFile(KernelPath, &KernelVirtualAddress);
        if (!KernelPhysicalAddress) {
            free(KernelPath);
            break;
        }
    } while (0);

    BmPollKey();
}
