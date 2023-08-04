/* SPDX-FileCopyrightText: (C) 2023 ilmmatias
 * SPDX-License-Identifier: BSD-3-Clause */

#include <boot.h>
#include <device.h>
#include <stdio.h>

void *__open(const char *path, int mode);

/*-------------------------------------------------------------------------------------------------
 * PURPOSE:
 *     This function is the BOOTMGR architecture-independent entry point.
 *     We detect and initialize all required hardware, show the boot menu, load up the OS, and
 *     transfer control to it.
 *
 * PARAMETERS:
 *     BootBlock - Information obtained while setting up the boot environment.
 *
 * RETURN VALUE:
 *     Does not return.
 *-----------------------------------------------------------------------------------------------*/
[[noreturn]] void BmMain(void *BootBlock) {
    BmInitDisplay();
    BmInitArch(BootBlock);
    BmInitMemory(BootBlock);

    do {
        DeviceContext *File = __open("bios()/open_this/flag.txt", 0);
        if (!File) {
            printf("__open() failed\n");
            break;
        }

        char FileData[54];
        if (!BiReadDevice(File, FileData, 0, sizeof(FileData))) {
            printf("BiReadDevice() failed\n");
            break;
        }

        printf("%.*s\n", sizeof(FileData), FileData);
    } while (0);

    while (1)
        ;
}
